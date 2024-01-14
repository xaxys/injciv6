unit injectform;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs,StdCtrls,ExtCtrls;

type
  TFormInject = class(TForm)
    ButtonInject:TButton;
    ListWindows:TListBox;
    procedure ButtonInjectClick(Sender:TObject);
    procedure FormShow(Sender:TObject);
  private
    procedure InjectByPID(pid:UInt32);
  public

  end;

var
  FormInject: TFormInject;

implementation

uses
  Windows,LCLType,process;

type
  EOpenProc=class(Exception);
  EDoInject=class(Exception);

{$R *.lfm}

{ TFormInject }

function EnumVisibleWindow(handle:HWND; lpara:LPARAM):WINBOOL;stdcall;
var
  str:Array[Byte] of WideChar;
begin
  if IsWindowVisible(handle) then
  begin
    GetWindowTextW(handle,@str,255);
    if str<>'' then
    begin
      TListBox(lpara).Items.AddObject(str,TObject(handle));
    end;
  end;
  Result:=True;
end;

procedure TFormInject.FormShow(Sender:TObject);
begin
  ListWindows.Clear;
  EnumWindows(@EnumVisibleWindow,LPARAM(ListWindows));
end;

function IsProcess32Bit(pid:UInt32):Boolean;
var
  iswow64:BOOL;
  hproc:HANDLE;
begin
  // 编译到x86的时候
{$ifdef CPUX86}
  IsWow64Process(GetCurrentProcess,@iswow64);
  // 如果当前进程运行在wow64，那么说明当前系统是64位的
  if iswow64 then
  begin
    hproc:=OpenProcess(PROCESS_QUERY_INFORMATION,False,pid);
    if hproc=0 then
      raise EOpenProc.Create('无法打开进程');
    IsWow64Process(hproc,@iswow64);
    CloseHandle(hproc);
    Result:=iswow64;
  end
  else
    Result:=True;
{$endif}
  // 编译到x64
{$ifdef CPUX64}
  hproc:=OpenProcess(PROCESS_QUERY_INFORMATION,False,pid);
  if hproc=0 then
    raise EOpenProc.Create('无法打开进程');
  IsWow64Process(hproc,@iswow64);
  CloseHandle(hproc);
  Result:=iswow64;
{$endif}
end;

function TryInject(const injector:String; pid:UInt32):Boolean;
var
  runproc:TProcess;
  output:TStringList;
begin
  Result:=False;
  runproc:=TProcess.Create(nil);
  output:=TStringList.Create;
  try
    runproc.Executable:=injector;
    runproc.Parameters.Add('-i=%d',[pid]);
    runproc.Options:=runproc.Options+[poWaitOnExit,poUsePipes];
    runproc.Execute;
    if runproc.ExitCode=0 then
      Exit(True);
    output.LoadFromStream(runproc.Output);
    raise EDoInject.Create(output.Text);
  finally
    runproc.Free;
    output.Free;
  end;
end;

procedure TFormInject.InjectByPID(pid:UInt32);
var
  is32:Boolean;
  exedir:String;
  hookdllpath:String;
  injectorpath:String;
begin
  try
    is32:=IsProcess32Bit(pid);
    exedir:=ExtractFileDir(Application.ExeName)+DirectorySeparator;
    if is32 then
    begin
      hookdllpath:=exedir+'hookdll32.dll';
      injectorpath:=exedir+'injector32.exe';
    end
    else begin
      hookdllpath:=exedir+'hookdll64.dll';
      injectorpath:=exedir+'injector64.exe';
    end;
    if not FileExists(hookdllpath) then
    begin
      Application.MessageBox(PChar(Format('找不到"%s"',[hookdllpath])),'错误',MB_ICONERROR);
      Exit;
    end;
    if not FileExists(injectorpath) then
    begin
      Application.MessageBox(PChar(Format('找不到"%s"',[injectorpath])),'错误',MB_ICONERROR);
      Exit;
    end;
    if TryInject(injectorpath,pid) then
      Application.MessageBox('注入成功','提示',MB_OK);
  except
    on E:EOpenProc do
      Application.MessageBox('无法判断进程位数，如果不是管理员权限，请以管理员身份重新启动','错误',MB_ICONERROR);
    on E:EDoInject do
      Application.MessageBox(PChar(Format('注入失败，原因如下："%s"',[E.Message])),'错误',MB_ICONERROR);
  end;
end;

procedure TFormInject.ButtonInjectClick(Sender:TObject);
var
  hwnd:THandle;
  pid:DWORD;
begin
  if ListWindows.ItemIndex>=0 then
  begin
    hwnd:=THandle(ListWindows.Items.Objects[ListWindows.ItemIndex]);
    GetWindowThreadProcessId(hwnd,@pid);
    if pid<>0 then
      InjectByPID(pid)
    else
      Application.MessageBox('该进程已关闭','错误',MB_ICONERROR);
  end;
end;

end.
