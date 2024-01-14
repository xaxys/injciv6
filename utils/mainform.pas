unit mainform;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs,StdCtrls;

type
  TFormain = class(TForm)
    ButtonInject:TButton;
    ButtonLAN:TButton;
    procedure ButtonInjectClick(Sender:TObject);
    procedure ButtonLANClick(Sender:TObject);
    procedure FormCreate(Sender:TObject);
  private

  public

  end;

var
  Formain: TFormain;

implementation

uses
  injectform,lantestform;

{$R *.lfm}

{ TFormain }

procedure TFormain.FormCreate(Sender:TObject);
begin
  PixelsPerInch:=96;
end;

procedure TFormain.ButtonInjectClick(Sender:TObject);
begin
  if not FormInject.Showing then
  begin
    FormInject.Left:=Self.Left+100;
    FormInject.Top:=Self.Top-100;
    FormInject.Show;
  end;
  FormInject.SetFocus;
end;

procedure TFormain.ButtonLANClick(Sender:TObject);
begin
  if not FormLANTest.Showing then
  begin
    FormLANTest.Left:=Self.Left+200;
    FormLANTest.Top:=Self.Top-100;
    FormLANTest.Show;
  end;
  FormLANTest.SetFocus;
end;

end.
