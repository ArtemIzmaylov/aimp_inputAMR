unit AIMP.InputAMR.Plugin;

interface

uses
  Windows,
  // API
  apiPlugin,
  apiCore,
  apiObjects,
  apiDecoders,
  apiFileManager,
  apiWrappers,
  // Plugin
  AIMP.InputAMR.Decoder,
  AIMPCustomPlugin;

type

  { TAIMPDecoderAMR }

  TAIMPDecoderAMR = class(TInterfacedObject, IAIMPAudioDecoder)
  strict private
    FDecoder: TAMRDecoder;
    FStream: TAIMPStreamWrapper;
  protected
    property Decoder: TAMRDecoder read FDecoder;
  public
    constructor Create(AStream: IAIMPStream);
    destructor Destroy; override;
    // IAIMPAudioDecoder
    function GetAvailableData: Int64; stdcall;
    function GetFileInfo(FileInfo: IAIMPFileInfo): LongBool; stdcall;
    function GetPosition: Int64; stdcall;
    function GetSize: Int64; stdcall;
    function GetStreamInfo(out SampleRate, Channels, SampleFormat: Integer): LongBool; stdcall;
    function IsRealTimeStream: LongBool; stdcall;
    function IsSeekable: LongBool; stdcall;
    function Read(ABuffer: PByte; ACount: Integer): Integer; stdcall;
    function SetPosition(const Value: Int64): LongBool; stdcall;
  end;

  { TAIMPDecoderAMRInfo }

  TAIMPDecoderAMRInfo = class(TInterfacedObject, IAIMPExtensionAudioDecoder)
  public
    // IAIMPExtensionAudioDecoder
    function CreateDecoder(Stream: IAIMPStream; Flags: Cardinal;
      ErrorInfo: IAIMPErrorInfo; out Decoder: IAIMPAudioDecoder): HRESULT; stdcall;
  end;

  { TAIMPFileFormatAMR }

  TAIMPFileFormatAMR = class(TInterfacedObject, IAIMPExtensionFileFormat)
  public
    // IAIMPExtensionFileFormat
    function GetDescription(out S: IAIMPString): HRESULT; stdcall;
    function GetExtList(out S: IAIMPString): HRESULT; stdcall;
    function GetFlags(out Flags: Cardinal): HRESULT; stdcall;
  end;

  { TAIMPPluginAMR }

  TAIMPPluginAMR = class(TAIMPCustomPlugin)
  protected
    function InfoGet(Index: Integer): PWideChar; override; stdcall;
    function InfoGetCategories: Cardinal; override; stdcall;
    function Initialize(Core: IAIMPCore): HRESULT; override; stdcall;
  end;

function AIMPPluginGetHeader(out Header: IAIMPPlugin): HRESULT; stdcall;
implementation

uses
  Math, SysUtils, ACL.Classes.ByteBuffer, ACL.FastCode;

function AIMPPluginGetHeader(out Header: IAIMPPlugin): HRESULT; stdcall;
begin
  try
    Header := TAIMPPluginAMR.Create;
    Result := S_OK;
  except
    Result := E_UNEXPECTED;
  end;
end;

{ TAIMPDecoderAMR }

constructor TAIMPDecoderAMR.Create(AStream: IAIMPStream);
begin
  inherited Create;
  FStream := TAIMPStreamWrapper.Create(AStream);
  FDecoder := TAMRDecoder.Create(FStream);
end;

destructor TAIMPDecoderAMR.Destroy;
begin
  FreeAndNil(FDecoder);
  FreeAndNil(FStream);
  inherited Destroy;
end;

function TAIMPDecoderAMR.GetAvailableData: Int64;
begin
  Result := GetSize - GetPosition;
end;

function TAIMPDecoderAMR.GetFileInfo(FileInfo: IAIMPFileInfo): LongBool;
begin
  Result := False;
end;

function TAIMPDecoderAMR.GetPosition: Int64;
begin
  Result := Decoder.Frame * TAMRDecoder.BytesPerFrame;
end;

function TAIMPDecoderAMR.GetSize: Int64;
begin
  Result := Decoder.Frames * TAMRDecoder.BytesPerFrame;
end;

function TAIMPDecoderAMR.GetStreamInfo(out SampleRate, Channels, SampleFormat: Integer): LongBool;
begin
  SampleRate := TAMRDecoder.SampleRate;
  Channels := TAMRDecoder.Channels;
  SampleFormat := AIMP_DECODER_SAMPLEFORMAT_16BIT;
  Result := True;
end;

function TAIMPDecoderAMR.IsRealTimeStream: LongBool;
begin
  Result := False;
end;

function TAIMPDecoderAMR.IsSeekable: LongBool;
begin
  Result := True;
end;

function TAIMPDecoderAMR.Read(ABuffer: PByte; ACount: Integer): Integer;
var
  ABytesToMove: Integer;
begin
  Result := 0;
  while ACount > 0 do
  begin
    if Decoder.BufferPCM.Used = 0 then
    begin
      if not Decoder.ReadFrameAndDecodeIt then
        Break;
    end;
    ABytesToMove := Min(ACount, Decoder.BufferPCM.Used);
    FastMove(Decoder.BufferPCM.Data^, ABuffer^, ABytesToMove);
    Decoder.BufferPCM.Remove(ABytesToMove);
    Inc(ABuffer, ABytesToMove);
    Dec(ACount, ABytesToMove);
    Inc(Result, ABytesToMove);
  end;
end;

function TAIMPDecoderAMR.SetPosition(const Value: Int64): LongBool;
var
  AFrame: Integer;
begin
  AFrame := Value div TAMRDecoder.BytesPerFrame;
  Result := InRange(AFrame, 0, Decoder.Frames - 1);
  if Result then
    Decoder.Frame := AFrame;
end;

{ TAIMPDecoderAMRInfo }

function TAIMPDecoderAMRInfo.CreateDecoder(Stream: IAIMPStream;
  Flags: Cardinal; ErrorInfo: IAIMPErrorInfo; out Decoder: IAIMPAudioDecoder): HRESULT;
begin
  try
    Decoder := TAIMPDecoderAMR.Create(Stream);
    Result := S_OK;
  except
    Result := E_FAIL;
  end;
end;

{ TAIMPFileFormatAMR }

function TAIMPFileFormatAMR.GetDescription(out S: IAIMPString): HRESULT;
begin
  S := MakeString('Adaptive Multi-Rate Files');
  Result := S_OK;
end;

function TAIMPFileFormatAMR.GetExtList(out S: IAIMPString): HRESULT;
begin
  S := MakeString('*.amr;');
  Result := S_OK;
end;

function TAIMPFileFormatAMR.GetFlags(out Flags: Cardinal): HRESULT;
begin
  Flags := AIMP_SERVICE_FILEFORMATS_CATEGORY_AUDIO;
  Result := S_OK;
end;

{ TAIMPPluginAMR }

function TAIMPPluginAMR.InfoGet(Index: Integer): PWideChar;
begin
  case Index of
    AIMP_PLUGIN_INFO_NAME:
      Result := 'AMR-NB Decoder v1.0 Beta';
    AIMP_PLUGIN_INFO_AUTHOR:
      Result := 'Artem Izmaylov';
    AIMP_PLUGIN_INFO_SHORT_DESCRIPTION:
      Result := 'Based on OpenCore-AMR Library';
    AIMP_PLUGIN_INFO_FULL_DESCRIPTION:
      Result := '*.amr;'#13#10'http://sourceforge.net/projects/opencore-amr/';
  else
    Result := nil;
  end;
end;

function TAIMPPluginAMR.InfoGetCategories: Cardinal;
begin
  Result := AIMP_PLUGIN_CATEGORY_DECODERS;
end;

function TAIMPPluginAMR.Initialize(Core: IAIMPCore): HRESULT;
begin
  Result := inherited Initialize(Core);
  if Succeeded(Result) then
  begin
    Core.RegisterExtension(IID_IAIMPServiceAudioDecoders, TAIMPDecoderAMRInfo.Create);
    Core.RegisterExtension(IID_IAIMPServiceFileFormats, TAIMPFileFormatAMR.Create);
  end;
end;

end.
