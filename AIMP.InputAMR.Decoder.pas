unit AIMP.InputAMR.Decoder;

interface

uses
  SysUtils, Generics.Collections, Classes, ACL.Classes.ByteBuffer;

type
  TAMRHeaderID = array [0..5] of AnsiChar;

const
  AMR_HEADER_ID = '#!AMR'#10;

type

  { EAMRDecoderError }

  EAMRDecoderError = class(Exception);

  { TAMRDecoder }

  TAMRDecoder = class
  strict private const
    FrameDuration = 20; // msec
    FrameMaxSize = 32;
    FrameSizes: array[0..15] of Byte = (12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0);

  public const
    BitDepth = 16;
    Channels = 1;
    SampleRate = 8000;
    BytesPerFrame = SampleRate * Channels * BitDepth div 8 * FrameDuration div 1000;

  strict private
    FBufferARM: TACLByteBuffer;
    FBufferPCM: TACLRemovableByteBuffer;
    FFrames: Integer;
    FHandle: Pointer;
    FSeekTable: TList<Int64>;
    FStream: TStream;

    function GetFrame: Integer;
    procedure SetFrame(const Value: Integer);
  protected
    procedure CalculateDurationAndSeekTable;
    procedure CheckFileHeader;

    property SeekTable: TList<Int64> read FSeekTable;
    property Stream: TStream read FStream;
  public
    constructor Create(AStream: TStream);
    destructor Destroy; override;
    function ReadFrameAndDecodeIt: Boolean;

    property BufferARM: TACLByteBuffer read FBufferARM;
    property BufferPCM: TACLRemovableByteBuffer read FBufferPCM;
    property Frame: Integer read GetFrame write SetFrame;
    property Frames: Integer read FFrames;
  end;

implementation

const
  sErrorCreateDecoder = 'Cannot create the decoder';
  sErrorReadData = 'Unexpected end of file';
  sErrorUnsupportedFileFormat = 'File is not valid AMR file format';

const
  LibCoreAMR = 'opencore_amr.dll';

function Decoder_Interface_init: Pointer; cdecl external LibCoreAMR;
procedure Decoder_Interface_Decode(State: Pointer; Input: PByte; Output: PShortInt; Flags: Integer); cdecl external LibCoreAMR;
procedure Decoder_Interface_exit(State: Pointer); cdecl external LibCoreAMR;

{ TAMRDecoder }

constructor TAMRDecoder.Create(AStream: TStream);
begin
  inherited Create;
  FStream := AStream;
  FSeekTable := TList<Int64>.Create;
  FBufferARM := TACLByteBuffer.Create(FrameMaxSize);
  FBufferPCM := TACLRemovableByteBuffer.Create(BytesPerFrame);

  CheckFileHeader;
  CalculateDurationAndSeekTable;

  FHandle := Decoder_Interface_init;
  if FHandle = nil then
    raise EAMRDecoderError.Create(sErrorCreateDecoder);
end;

destructor TAMRDecoder.Destroy;
begin
  if FHandle <> nil then
  begin
    Decoder_Interface_exit(FHandle);
    FHandle := nil;
  end;
  FreeAndNil(FBufferARM);
  FreeAndNil(FBufferPCM);
  FreeAndNil(FSeekTable);
  inherited Destroy;
end;

function TAMRDecoder.ReadFrameAndDecodeIt: Boolean;
var
  AFrameSize: Integer;
begin
  try
    Stream.ReadBuffer(BufferARM.DataArr^[0], 1);
    AFrameSize := FrameSizes[(BufferARM.Data^ shr 3) and $F];
    BufferARM.Used := 1 + AFrameSize;
    Stream.ReadBuffer(BufferARM.DataArr^[1], AFrameSize);
    Decoder_Interface_Decode(FHandle, BufferARM.Data, PShortInt(BufferPCM.Data), 0);
    BufferPCM.Used := BufferPCM.Size;
    Result := True;
  except
    Result := False;
  end;
end;

procedure TAMRDecoder.CalculateDurationAndSeekTable;
var
  AFrameID: Byte;
begin
  while Stream.Read(AFrameID, SizeOf(AFrameID)) = SizeOf(AFrameID) do
  begin
    SeekTable.Add(Stream.Position - SizeOf(AFrameID));
    Stream.Seek(FrameSizes[(AFrameID shr 3) and $F], soCurrent);
    Inc(FFrames);
  end;
  SeekTable.Add(Stream.Size);
  Frame := 0;
end;

procedure TAMRDecoder.CheckFileHeader;
var
  ID: TAMRHeaderID;
begin
  if Stream.Read(ID, SizeOf(ID)) <> SizeOf(ID) then
    raise EAMRDecoderError.Create(sErrorReadData);
  if ID <> AMR_HEADER_ID then
    raise EAMRDecoderError.Create(sErrorUnsupportedFileFormat);
end;

function TAMRDecoder.GetFrame: Integer;
var
  APosition: Int64;
  I: Integer;
begin
  Result := -1;
  APosition := Stream.Position;
  for I := 0 to SeekTable.Count - 1 do
  begin
    if SeekTable.List[I] <= APosition then
      Result := I
    else
      Break;
  end;
end;

procedure TAMRDecoder.SetFrame(const Value: Integer);
begin
  Stream.Position := SeekTable[Value];
  BufferARM.Flush(False);
  BufferPCM.Flush(False);
end;

end.
