unit lantestform;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs,StdCtrls,ExtCtrls,
  sockets,winsock2,AVL_Tree;

type

  TRecvThread = class;

  TFormLANTest = class(TForm)
    ButtonBroadTest:TButton;
    MemoLog:TMemo;
    PanelTools:TPanel;
    procedure ButtonBroadTestClick(Sender:TObject);
    procedure FormCreate(Sender:TObject);
    procedure FormDestroy(Sender:TObject);
  private
    thread:TRecvThread;
  public
    procedure Log(const S:String);
  end;

  TRecvThread = class(TThread)
  private type
    TAddrRec = record
      addr:TInAddr;
      tick:QWord;
    end;
    PAddrRec = ^TAddrRec;
    TInAddrArray = Array of TInAddr;
  private
    socks:TFDSet;
    form:TFormLANTest;
    RemoteAddrs:TAVLTree;
    LocalAddrs:TInAddrArray;
    log:String;
    procedure LogRecvInfo;
    class function CmpAddrRec(p1,p2:Pointer):Integer;static;
    procedure UpdateLocalAddrs;
    function IsLocalAddr(const Addr:TInAddr):Boolean;
  protected
    procedure Execute; override;
  public
    constructor Create;
    destructor Destroy; override;
  end;

  TTestThread = class(TThread)
  private
    sock:TSocket;
    form:TFormLANTest;
    log:String;
    procedure LogInfo;
    procedure UnlockButton;
  protected
    procedure Execute; override;
  public
    constructor Create(AForm:TFormLANTest);
    destructor Destroy; override;
  end;

var
  FormLANTest: TFormLANTest;

implementation

uses
  Windows;

const
  SOCK_START_PORT = 6666;

var
  QPF_Freq:TLargeInteger;

{$R *.lfm}

{ TFormLANTest }

procedure TFormLANTest.FormCreate(Sender:TObject);
begin
  thread:=TRecvThread.Create;
  thread.form:=Self;
end;

procedure TFormLANTest.ButtonBroadTestClick(Sender:TObject);
begin
  ButtonBroadTest.Enabled:=False;
  TTestThread.Create(Self);
end;

procedure TFormLANTest.FormDestroy(Sender:TObject);
begin
  thread.Terminate;
  thread.Free;
end;

procedure TFormLANTest.Log(const S:String);
begin
  MemoLog.Lines.Add(TimeToStr(Now)+': '+S);
end;

{ TRecvThread }

procedure TRecvThread.LogRecvInfo;
begin
  form.Log(log);
end;

class function TRecvThread.CmpAddrRec(p1,p2:Pointer):Integer;
begin
  Result:=PAddrRec(p1)^.addr.S_addr-PAddrRec(p2)^.addr.S_addr;
end;

procedure TRecvThread.UpdateLocalAddrs;
var
  phost:PHostEnt;
  len,i:Integer;
  ppc:PPChar;
begin
  phost:=gethostbyname('');
  len:=0;
  ppc:=phost^.h_addr;
  while ppc^<>nil do
  begin
    Inc(len);
    Inc(ppc);
  end;
  if len=Length(LocalAddrs) then
    Exit;
  SetLength(LocalAddrs,len);
  ppc:=phost^.h_addr;
  i:=0;
  while i<len do
  begin
    Move(ppc^^,LocalAddrs[i],4);
    Inc(ppc);
    Inc(i);
  end;
end;

function TRecvThread.IsLocalAddr(const Addr:TInAddr):Boolean;
var
  i:Integer;
begin
  for i:=0 to High(LocalAddrs) do
    if LocalAddrs[i].S_addr=Addr.S_addr then
      Exit(True);
  Result:=False;
end;

procedure TRecvThread.Execute;
var
  i,fromlen:Integer;
  buf,buf2:Cardinal;
  fromaddr:TSockAddr;
  recvsock:TSocket;
  tmpfds:TFDSet;
  lasttick,tick:QWord;
  node:TAVLTreeNode;
  paddr:PAddrRec;
  recved:Boolean;
begin
  lasttick:=GetTickCount64;
  while not Terminated do
  begin
    tmpfds:=socks;
    select(0,@tmpfds,nil,nil,nil);
    tick:=GetTickCount64;
    if tick-lasttick>5000 then
      UpdateLocalAddrs;
    for i:=0 to tmpfds.fd_count-1 do
    begin
      buf:=0;
      recved:=False;
      recvsock:=tmpfds.fd_array[i];
      fromaddr.sin_family:=AF_INET;
      fromaddr.sin_addr.S_addr:=INADDR_ANY;
      fromaddr.sin_port:=0;
      fromlen:=SizeOf(fromaddr);
      recvfrom(recvsock,@buf,4,0,@fromaddr,@fromlen);
      if buf<>1 then
        Continue;
      node:=RemoteAddrs.Find(@fromaddr.sin_addr);
      if node<>nil then
      begin
        paddr:=node.Data;
        if tick-paddr^.tick>1000 then
        begin
          paddr^.tick:=tick;
          recved:=True;
        end;
      end
      else begin
        node:=TAVLTreeNode.Create;
        GetMem(paddr,sizeof(TAddrRec));
        paddr^.addr:=fromaddr.sin_addr;
        paddr^.tick:=tick;
        node.Data:=paddr;
        RemoteAddrs.Add(node);
        recved:=True;
      end;
      if not recved then
        Continue;
      if IsLocalAddr(fromaddr.sin_addr) then
      begin
        log:=Format('通过%s发送了测试广播',[inet_ntoa(fromaddr.sin_addr)]);
        Synchronize(@LogRecvInfo);
        //buf2:=2;
        //sendto(recvsock,@buf2,4,0,@fromaddr,sizeof(fromaddr));
      end
      else begin
        log:=Format('收到了来自%s的测试',[inet_ntoa(fromaddr.sin_addr)]);
        Synchronize(@LogRecvInfo);
        buf2:=2;
        sendto(recvsock,@buf2,4,0,@fromaddr,sizeof(fromaddr));
      end;
    end;
    lasttick:=tick;
  end;
end;

constructor TRecvThread.Create;
var
  i:Integer;
  sock:TSocket;
  addr:TSockAddr;
begin
  addr.sin_family:=AF_INET;
  addr.sin_addr.S_addr:=INADDR_ANY;
  socks.fd_count:=0;
  for i:=0 to 9 do
  begin
    sock:=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    addr.sin_port:=htons(SOCK_START_PORT+i);
    if bind(sock,addr,sizeof(addr))<>-1 then
      FD_SET(sock,socks);
  end;
  RemoteAddrs:=TAVLTree.Create(@CmpAddrRec);
  UpdateLocalAddrs;
  inherited Create(False);
end;

destructor TRecvThread.Destroy;
var
  i:Integer;
  node:TAVLTreeNode;
begin
  for i:=0 to socks.fd_count-1 do
    closesocket(socks.fd_array[i]);
  for node in RemoteAddrs do
    Freemem(node.Data);
  RemoteAddrs.Free;
  inherited Destroy;
end;

{ TTestThread }

procedure TTestThread.LogInfo;
begin
  form.Log(log);
end;

procedure TTestThread.UnlockButton;
begin
  form.ButtonBroadTest.Enabled:=True;
end;

procedure Delay_us(us:Integer);
var
  qpc,qpc2:TLargeInteger;
  deltatime:Int64;
begin
  QueryPerformanceCounter(@qpc);
  repeat
    QueryPerformanceCounter(@qpc2);
    deltatime:=Round(1000000*(qpc2-qpc)/QPF_Freq);
  until deltatime>=us;
end;

procedure TTestThread.Execute;
var
  i,res:Integer;
  addrto:TSockAddr;
  buf:Cardinal;
  timeout:TTimeVal;
  fromaddr:TSockAddr;
  fromlen:LongInt;
  tmpfds:TFDSet;
begin
  buf:=1;
  log:='正在发送测试广播...';
  Synchronize(@LogInfo);
  addrto.sin_family:=AF_INET;
  addrto.sin_addr.s_addr:=INADDR_BROADCAST;
  for i:=SOCK_START_PORT to SOCK_START_PORT+9 do
  begin
    addrto.sin_port:=htons(i);
    sendto(sock,@buf,4,0,@addrto,sizeof(addrto));
    Delay_us(100); // 延迟100微秒，否则可能会发不出去
  end;
  timeout.tv_sec:=1;
  timeout.tv_usec:=0;
  repeat
    tmpfds.fd_count:=0;
    FD_SET(sock,tmpfds);
    res:=select(0,@tmpfds,nil,nil,@timeout);
    if FD_ISSET(sock,tmpfds) then
    begin
      fromlen:=sizeof(fromaddr);
      recvfrom(sock,@buf,4,0,@fromaddr,@fromlen);
      if buf=2 then
      begin
        log:=Format('收到了来自%s的回复',[inet_ntoa(fromaddr.sin_addr)]);
        Synchronize(@LogInfo);
      end;
    end;
  until res<=0;
  Synchronize(@UnlockButton);
end;

constructor TTestThread.Create(AForm:TFormLANTest);
var
  opt:LongInt;
  name:TSockAddr;
begin
  form:=AForm;
  opt:=1;
  sock:=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  setsockopt(sock,SOL_SOCKET,SO_BROADCAST,opt,4);
  setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,opt,4);
  name.sin_family:=AF_INET;
  name.sin_addr.S_addr:=INADDR_ANY;
  name.sin_port:=0;
  bind(sock,@name,sizeof(name));
  FreeOnTerminate:=True;
  inherited Create(False);
end;

destructor TTestThread.Destroy;
begin
  closesocket(sock);
  inherited Destroy;
end;

initialization

QueryPerformanceFrequency(@QPF_Freq);

end.
