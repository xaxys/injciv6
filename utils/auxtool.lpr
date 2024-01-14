program auxtool;

{$mode objfpc}{$H+}

uses
  {$IFDEF UNIX}
  cthreads,
  {$ENDIF}
  {$IFDEF HASAMIGA}
  athreads,
  {$ENDIF}
  Interfaces, // this includes the LCL widgetset
  Forms, mainform, injectform, lantestform
  { you can add units after this };

{$R *.res}

begin
  RequireDerivedFormResource:=True;
  Application.MainFormOnTaskBar:=True;
  Application.Scaled:=True;
  Application.Initialize;
  Application.CreateForm(TFormain,Formain);
  Application.CreateForm(TFormInject,FormInject);
  Application.CreateForm(TFormLANTest,FormLANTest);
  Application.Run;
end.

