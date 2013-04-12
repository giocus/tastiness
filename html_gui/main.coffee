ws = new WebSocket("ws://localhost:9999/service")
ws.onopen = () ->
ws.onmessage = (e) ->
  b = new Blob([e.data], { type: "text/plain" })
  f = new FileReader()
  f.readAsText(b)
  f.onloadend = () ->
    console.log f.result
ws.onclose = () ->
  console.log 'close'

$('input[type=file]').on 'change', (e) ->
  f = e.target.files[0]
  if f
    b = new Blob(['openRom:',f])
    ws.send b

