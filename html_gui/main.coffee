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

ScreenCanvas = $('#screenCanvas')[0]
ScreenContext = ScreenCanvas.getContext '2d'
ScreenContext.wekbitImageSmoothingEnabled = false
ScreenImageData = ScreenContext.createImageData(256,224)
CurFrame = 0
FrameCount = 10000
FrameInputBytes = new Uint8Array(FrameCount)
FrameInputBytesChanged = false

FrameScrollerCount = 47
FrameScrollerNumbers = []
FrameScrollerInputBits = []
FrameScrollerFocus = 5

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
        else
          console.log "unknown message:",m
      catch e
        console.log "unknown string: #{e.data}"
  else
    if false
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
      frameCountInput.value = ''+inputLines.length
      frameCountChanged()
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

    if i == FrameScrollerFocus
      $(frameNumDiv).addClass 'focus'
      $(frameInputDiv).addClass 'focus'
  f(i)

InputBits = "ABSTUDLR"
frameN = document.getElementById 'frameN'
frameChanged = (v) ->
  if not v?
    v = parseInt(frameN.value)
  frame = Math.round(v)
  frame = 0 if isNaN(frame)
  frame = 0 if frame<1
  frameN.value = ''+frame
  if frame > FrameCount
    frameCountInput.value = '' + frame
    frameCountChanged()
    return

  if FrameInputBytesChanged
    FrameInputBytesChanged = false
    conn.send new Blob(['setFrameInputs:',FrameInputBytes])
  conn.send new Blob(['getFrame:'+frameN.value])
  $('#timelineControls #timepos').css { left: (512*(frame/FrameCount))+'px' }

  _.each FrameScrollerNumbers, (frameNumDiv, i) ->
    n = i + frame - FrameScrollerFocus
    if n<0
      frameNumDiv.innerText = ' '
      _.each FrameScrollerInputBits[i], (div) ->
        div.innerText = ' '
    else
      frameNumDiv.innerText = ''+n
      _.each FrameScrollerInputBits[i], (div, b) ->
        bitOn = 0 != (FrameInputBytes[n] & (1<<b))
        div.innerText = if bitOn then InputBits[b] else '.'

$(frameN).on 'change', frameChanged

frameCountInput = document.getElementById 'frameCount'
frameCountChanged = () ->
  FrameCount = parseInt frameCountInput.value
  FrameCount = 1 if isNaN FrameCount
  FrameCount = 1 if FrameCount<1
  oldArray = FrameInputBytes
  FrameInputBytes = new Uint8Array(FrameCount)
  FrameInputBytes.set(oldArray)
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

frameChanged()
