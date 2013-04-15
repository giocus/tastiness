gNextBinaryMessageType = null
RamBytesPerLine = 64

hexnum = (num, digits) ->
  result = num.toString(16)
  while result.length < digits
    result = "0" + result
  return result

$RamSpans = null
RamSpans = []
createRamSpans = () ->
  ramDiv = $('#ram')[0]
  while ramDiv.hasChildNodes()
    ramDiv.removeChild(ramDiv.childNodes[0])
  offset = 0
  while offset < 0x800
    for i in [0...RamBytesPerLine]
      if (i&7) == 0
        $("<span class='ramLabel'>"+hexnum(offset+i, 3)+" </span>").appendTo(ramDiv)
      RamSpans[offset+i] = $("<span>00</span>").appendTo(ramDiv)[0]
      if (i&3) == 3
        ramDiv.appendChild(document.createTextNode(' '))
    $('<br>').appendTo(ramDiv)
    offset += RamBytesPerLine
  $RamSpans = $('span', ramDiv)

createRamSpans()

OldRam = new Uint8Array(0x800)
OldRamHash = null

ScreenCanvas = $('#screenCanvas')[0]
ScreenContext = ScreenCanvas.getContext '2d'
ScreenContext.wekbitImageSmoothingEnabled = false
ScreenImageData = ScreenContext.createImageData(256,224)
CurFrame = 0
FrameCount = 0
FrameInputBytes = new Uint8Array(FrameCount)
FrameInputBytesChanged = false

RamBlackoutBytes = []
RamBlackoutBytesChanged = false

LuaSourceChanged = false

FrameScrollerCount = 47
FrameScrollerNumbers = []
FrameScrollerInputBits = []
FrameScrollerFocus = 5

InputLetters = "RLDUTSBA"
InputLetterBits = "ABSTUDLR" #InputLetters in bit order

ramBlackoutBytesChanged = () ->
  RamBlackoutBytes.sort (a,b) -> a-b
  RamBlackoutBytes = _.uniq RamBlackoutBytes
  $RamSpans.removeClass 'blackout'
  _.each RamBlackoutBytes, (i) ->
    $(RamSpans[i]).addClass 'blackout'

  RamBlackoutBytesChanged = true
  $('#ramBlackoutBytes').val(RamBlackoutBytes.join(','))
  frameChanged()

toggleRamByteBlackout = (ramOffset) ->
  if -1 == RamBlackoutBytes.indexOf ramOffset
    RamBlackoutBytes.push ramOffset
    $(RamSpans[ramOffset]).addClass 'blackout'
  else
    RamBlackoutBytes = _.without RamBlackoutBytes, ramOffset
    $(RamSpans[ramOffset]).removeClass 'blackout'
  ramBlackoutBytesChanged()

$('#ramBlackoutBytes').on 'change', (e) ->
  strs = $('#ramBlackoutBytes').val().split(/,/g)
  RamBlackoutBytes = _.map strs, (i) -> parseInt(i)
  RamBlackoutBytes = _.reject RamBlackoutBytes, isNaN
  ramBlackoutBytesChanged()

_.each RamSpans, (span, i) ->
  $(span).on 'click', (e) ->
    toggleRamByteBlackout i

copyTypedArray = (arr, con, len) ->
  newArray = new con(len)
  copyLen = Math.min(arr.length, len)
  for i in [0...copyLen]
    newArray[i] = arr[i]
  return newArray

setFrameInput = (frame, input) ->
  change = false
  if frame >= FrameInputBytes.length
    FrameInputBytes = copyTypedArray FrameInputBytes, Uint8Array, frame+100
    change = true
  if frame >= FrameCount
    FrameCount = frame+1
    change = true
  newv = input|0
  if FrameInputBytes[frame] != newv
    FrameInputBytes[frame] = newv
    change = true
  if change
    FrameInputBytesChanged = true
    frameChanged()

KeyStateSpans = {}
_.each InputLetters, (k) ->
  span = $("<span>#{k}</span>").appendTo('#keyState')
  KeyStateSpans[k] = span

KeyIsDown = {}
$(document).on 'keydown', (e) ->
  KeyIsDown[e.keyCode] = true
  updateInputStates()
$(document).on 'keyup', (e) ->
  KeyIsDown[e.keyCode] = false
  updateInputStates()

ERASE=1
WRITE=2
WritingInput = {}
KeyCodes = { R: 82, L: 76, D: 68, U: 85, T: 84, S: 83, B: 66, A: 65, LSHIFT: 16, RSHIFT: 13 }
updateInputStates = () ->
  erasing = KeyIsDown[KeyCodes.LSHIFT] || KeyIsDown[KeyCodes.RSHIFT]
  _.each InputLetters, (k) ->
    if KeyIsDown[KeyCodes[k]]
      if erasing
        KeyStateSpans[k].removeClass 'writing'
        KeyStateSpans[k].addClass 'erasing'
        WritingInput[k] = ERASE
      else
        KeyStateSpans[k].addClass 'writing'
        KeyStateSpans[k].removeClass 'erasing'
        WritingInput[k] = WRITE
    else
      KeyStateSpans[k].removeClass 'writing'
      KeyStateSpans[k].removeClass 'erasing'
      WritingInput[k] = 0

class Connection
  constructor: () ->
    @openSocket()
    @onmessage = () ->
    @opened = false

  openSocket: () =>
    @ws = new WebSocket("ws://localhost:9999/service")
    @ws.onopen = () =>
      console.log "socket opened"
      @opened = true
    @ws.onclose = () =>
      console.log "socket closed"
      @opened = false
      @openSocket()
    @ws.onerror = (e) ->
      console.log "socket error", e
    @ws.onmessage = (e) =>
      @onmessage e

  send: (m) =>
    return unless @opened
    @ws.send m

conn = new Connection()
conn.onmessage = (e) ->
  if typeof(e.data) == 'string'
    if e.data.match /^bin/
      gNextBinaryMessageType = e.data
    else
      try
        m = JSON.parse e.data
        if false
        else if m.t == 'ramHash'
          $('#ramHash')[0].innerText = m.ramHash
          if OldRamHash != m.ramHash
            $('#ramHash').addClass 'changed'
          else
            $('#ramHash').removeClass 'changed'
          OldRamHash = m.ramHash
        else
          console.log "unknown message:",m
      catch e
        console.log "unknown string: #{e.data}"
  else
    if false
    else if gNextBinaryMessageType == 'binInputs'
      fr = new FileReader()
      fr.readAsArrayBuffer(e.data)
      fr.onloadend = () ->
        ab = fr.result
        FrameInputBytes = new Uint8Array(ab)
        frameCountChanged(FrameInputBytes.length)
        frameChanged()

    else if gNextBinaryMessageType == 'binFrameGfx'
      fr = new FileReader()
      fr.readAsArrayBuffer(e.data)
      fr.onloadend = () ->
        ab = fr.result
        ScreenImageData.data.set(new Uint8Array(ab))
        ScreenContext.putImageData(ScreenImageData, 0, 0)

    else if gNextBinaryMessageType == 'binFrameRam'
      fr = new FileReader()
      fr.readAsArrayBuffer(e.data)
      $RamSpans.removeClass 'changed'
      fr.onloadend = () ->
        newRam = new Uint8Array(fr.result)
        for i in [0...0x800]
          if OldRam[i] != newRam[i]
            RamSpans[i].innerText = hexnum(newRam[i], 2)
            $(RamSpans[i]).addClass 'changed'
        OldRam = newRam

    else if gNextBinaryMessageType == 'binFrameLuaOutput'
      fr = new FileReader()
      fr.readAsText(e.data)
      fr.onloadend = () ->
        $('#luaOutput')[0].innerText = fr.result
    else
      console.log "unknown gNextBinaryMessageType: #{gNextBinaryMessageType}"

$('#romFile').on 'change', (e) ->
  f = e.target.files[0]
  if f
    b = new Blob(['openRom:',f])
    conn.send b
    frameChanged()

$('#fm2File').on 'change', (e) ->
  f = e.target.files[0]
  if f
    fr = new FileReader()
    fr.readAsText(f)
    fr.onloadend = () ->
      text = fr.result
      lines = text.split("\n")
      inputLines = _.filter lines, (l) -> l.match /^\|/
      frameCountChanged(inputLines.length)
      frameNumbers = $('#frameNumbers')[0]
      frameInputs = $('#frameInputs')[0]
      _.each inputLines, (inputLine, i) ->
        input = inputLine.match(/^...(.{8})/)[1]
        inputB = input
        inputB = inputB.replace /[^\.]/g, '1'
        inputB = inputB.replace /\./g, '0'
        FrameInputBytes[i] = parseInt(inputB, 2)
        FrameInputBytesChanged = true
      frameChanged()

frameNumbers = $('#frameNumbers')[0]
frameInputs = $('#frameInputs')[0]
for i in [0...FrameScrollerCount]
  f = (i) ->
    frameNumDiv = $("<div>#{i}</div>").appendTo(frameNumbers)[0]
    $(frameNumDiv).on 'click', (e) ->
      frameNum = parseInt frameNumDiv.innerHTML
      frameChanged frameNum
    FrameScrollerNumbers.push frameNumDiv

    frameInputDiv = $("<div></div>").appendTo(frameInputs)[0]
    inputBits = for j in [0...8]
      $("<div>.</div>").appendTo(frameInputDiv)[0]
    FrameScrollerInputBits.push inputBits

    $(frameInputDiv).on 'mousemove', (e) ->
      n = i + CurFrame - FrameScrollerFocus
      return if n<0
      _.each WritingInput, (v,k) ->
        bit = InputLetterBits.indexOf k
        if v == WRITE
          setFrameInput n, (FrameInputBytes[n]|0) | (1<<bit)
        else if v == ERASE
          setFrameInput n, (FrameInputBytes[n]|0) & ~(1<<bit)

    if i == FrameScrollerFocus
      $(frameNumDiv).addClass 'focus'
      $(frameInputDiv).addClass 'focus'
  f(i)

frameN = document.getElementById 'frameN'
frameChanged = (v) ->
  if not v?
    v = parseInt(frameN.value)
  CurFrame = v|0
  CurFrame = 0 if CurFrame<1
  frameN.value = ''+CurFrame
  if CurFrame > FrameCount
    frameCountInput.value = '' + CurFrame
    frameCountChanged()
    return

  if FrameInputBytesChanged
    FrameInputBytesChanged = false
    conn.send new Blob(['setFrameInputs:',FrameInputBytes])

  if RamBlackoutBytesChanged
    RamBlackoutBytesChanged = false
    blob = copyTypedArray(RamBlackoutBytes,Uint16Array,RamBlackoutBytes.length)
    conn.send new Blob(['setRamBlackoutBytes:',blob])

  if LuaSourceChanged
    LuaSourceChanged = false
    conn.send new Blob(['setLuaSource:',$('#luaSource').val()])

  conn.send new Blob(['getFrame:'+frameN.value])
  $('#timelineControls #timepos').css { left: (512*(CurFrame/FrameCount))+'px' }

  _.each FrameScrollerNumbers, (frameNumDiv, i) ->
    n = i + CurFrame - FrameScrollerFocus
    if n<0
      frameNumDiv.innerText = ' '
      _.each FrameScrollerInputBits[i], (div) ->
        div.innerText = ' '
    else
      frameNumDiv.innerText = ''+n
      _.each FrameScrollerInputBits[i], (div, b) ->
        bitOn = 0 != (FrameInputBytes[n] & (1<<b))
        div.innerText = if bitOn then InputLetterBits[b] else '.'

$(frameN).on 'change', frameChanged

frameCountInput = document.getElementById 'frameCount'
frameCountChanged = (v) ->
  if not v?
    v = parseInt(frameCountInput.value)
  FrameCount = v|0
  FrameCount = 1 if FrameCount<1
  FrameInputBytes = copyTypedArray FrameInputBytes, Uint8Array, FrameCount
  frameCountInput.value = ''+FrameCount
  frameChanged()
$(frameCountInput).on 'change', frameCountChanged

$('#timelineControls #timebar').on 'click', (e) ->
  frame = (e.offsetX/512) * FrameCount
  frameChanged(frame)

$('#timelineControls input[type=button]').on 'click', (e) ->
  delta = parseInt e.currentTarget.value
  frame = parseInt frameN.value
  frameChanged(frame+delta)

luaSourceChanged = () ->
  LuaSourceChanged = true
  frameChanged()


$('#luaSource').on 'keyup', luaSourceChanged
$('#luaSource').on 'change', luaSourceChanged

frameChanged()
