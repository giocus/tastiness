// Generated by CoffeeScript 1.4.0
(function() {
  var $RamSpans, AStarInitialized, Connection, CurFrame, ERASE, FrameCount, FrameInputBytes, FrameInputBytesChanged, FrameScrollerCount, FrameScrollerFocus, FrameScrollerInputBits, FrameScrollerNumbers, InputLetterBits, InputLetters, KeyCodes, KeyIsDown, KeyStateSpans, LuaSourceChanged, OldRam, OldRamHash, RamBlackoutBytes, RamBlackoutBytesChanged, RamBytesPerLine, RamSpans, ScreenCanvas, ScreenContext, ScreenImageData, WRITE, WritingInput, conn, copyTypedArray, createRamSpans, f, frameChanged, frameCountChanged, frameCountInput, frameInputs, frameN, frameNumbers, gNextBinaryMessageType, hexnum, i, inputStateToText, luaSourceChanged, ramBlackoutBytesChanged, sendFrameCommandsTimeout, setFrameInput, toggleRamByteBlackout, updateInputStates, _i,
    __bind = function(fn, me){ return function(){ return fn.apply(me, arguments); }; };

  gNextBinaryMessageType = null;

  RamBytesPerLine = 64;

  hexnum = function(num, digits) {
    var result;
    result = num.toString(16);
    while (result.length < digits) {
      result = "0" + result;
    }
    return result;
  };

  $RamSpans = null;

  RamSpans = [];

  createRamSpans = function() {
    var i, offset, ramDiv, _i;
    ramDiv = $('#ram')[0];
    while (ramDiv.hasChildNodes()) {
      ramDiv.removeChild(ramDiv.childNodes[0]);
    }
    offset = 0;
    while (offset < 0x800) {
      for (i = _i = 0; 0 <= RamBytesPerLine ? _i < RamBytesPerLine : _i > RamBytesPerLine; i = 0 <= RamBytesPerLine ? ++_i : --_i) {
        if ((i & 7) === 0) {
          $("<span class='ramLabel'>" + hexnum(offset + i, 3) + " </span>").appendTo(ramDiv);
        }
        RamSpans[offset + i] = $("<span>00</span>").appendTo(ramDiv)[0];
        if ((i & 3) === 3) {
          ramDiv.appendChild(document.createTextNode(' '));
        }
      }
      $('<br>').appendTo(ramDiv);
      offset += RamBytesPerLine;
    }
    return $RamSpans = $('span', ramDiv);
  };

  createRamSpans();

  OldRam = new Uint8Array(0x800);

  OldRamHash = null;

  ScreenCanvas = $('#screenCanvas')[0];

  ScreenContext = ScreenCanvas.getContext('2d');

  ScreenContext.wekbitImageSmoothingEnabled = false;

  ScreenImageData = ScreenContext.createImageData(256, 224);

  CurFrame = 0;

  FrameCount = 0;

  FrameInputBytes = new Uint8Array(FrameCount);

  FrameInputBytesChanged = false;

  RamBlackoutBytes = [];

  RamBlackoutBytesChanged = false;

  LuaSourceChanged = false;

  FrameScrollerCount = 47;

  FrameScrollerNumbers = [];

  FrameScrollerInputBits = [];

  FrameScrollerFocus = 5;

  InputLetters = "RLDUTSBA";

  InputLetterBits = "ABSTUDLR";

  ramBlackoutBytesChanged = function(blackoutBytes) {
    blackoutBytes.sort(function(a, b) {
      return a - b;
    });
    blackoutBytes = _.uniq(blackoutBytes);
    $RamSpans.removeClass('blackout');
    _.each(blackoutBytes, function(i) {
      return $(RamSpans[i]).addClass('blackout');
    });
    if (blackoutBytes.join(',') !== RamBlackoutBytes.join(',')) {
      RamBlackoutBytes = blackoutBytes;
      RamBlackoutBytesChanged = true;
      $('#ramBlackoutBytes').val(blackoutBytes.join(','));
      return frameChanged();
    }
  };

  toggleRamByteBlackout = function(ramOffset) {
    var blackoutBytes;
    blackoutBytes = RamBlackoutBytes.slice();
    if (-1 === blackoutBytes.indexOf(ramOffset)) {
      blackoutBytes.push(ramOffset);
      $(RamSpans[ramOffset]).addClass('blackout');
    } else {
      blackoutBytes = _.without(blackoutBytes, ramOffset);
      $(RamSpans[ramOffset]).removeClass('blackout');
    }
    return ramBlackoutBytesChanged(blackoutBytes);
  };

  $('#ramBlackoutBytes').on('change', function(e) {
    var bytes, strs;
    strs = $('#ramBlackoutBytes').val().split(/,/g);
    bytes = _.map(strs, function(i) {
      return parseInt(i);
    });
    bytes = _.reject(bytes, isNaN);
    return ramBlackoutBytesChanged(bytes);
  });

  _.each(RamSpans, function(span, i) {
    return $(span).on('click', function(e) {
      return toggleRamByteBlackout(i);
    });
  });

  copyTypedArray = function(arr, con, len) {
    var copyLen, i, newArray, _i;
    newArray = new con(len);
    copyLen = Math.min(arr.length, len);
    for (i = _i = 0; 0 <= copyLen ? _i < copyLen : _i > copyLen; i = 0 <= copyLen ? ++_i : --_i) {
      newArray[i] = arr[i];
    }
    return newArray;
  };

  setFrameInput = function(frame, input, forceFrameChanged) {
    var change, newv;
    change = false;
    if (frame >= FrameInputBytes.length) {
      FrameInputBytes = copyTypedArray(FrameInputBytes, Uint8Array, frame + 100);
      change = true;
    }
    if (frame >= FrameCount) {
      FrameCount = frame + 1;
      change = true;
    }
    newv = input | 0;
    if (FrameInputBytes[frame] !== newv) {
      FrameInputBytes[frame] = newv;
      FrameInputBytesChanged = true;
      change = true;
    }
    if (forceFrameChanged != null) {
      change = forceFrameChanged;
    }
    if (change) {
      return frameChanged();
    }
  };

  KeyStateSpans = {};

  _.each(InputLetters, function(k) {
    var span;
    span = $("<span>" + k + "</span>").appendTo('#keyState');
    return KeyStateSpans[k] = span;
  });

  KeyIsDown = {};

  $(document).on('keydown', function(e) {
    KeyIsDown[e.keyCode] = true;
    return updateInputStates();
  });

  $(document).on('keyup', function(e) {
    KeyIsDown[e.keyCode] = false;
    return updateInputStates();
  });

  ERASE = 1;

  WRITE = 2;

  WritingInput = {};

  KeyCodes = {
    R: 82,
    L: 76,
    D: 68,
    U: 85,
    T: 84,
    S: 83,
    B: 66,
    A: 65,
    LSHIFT: 16,
    RSHIFT: 13
  };

  updateInputStates = function() {
    var erasing;
    erasing = KeyIsDown[KeyCodes.LSHIFT] || KeyIsDown[KeyCodes.RSHIFT];
    return _.each(InputLetters, function(k) {
      if (KeyIsDown[KeyCodes[k]]) {
        if (erasing) {
          KeyStateSpans[k].removeClass('writing');
          KeyStateSpans[k].addClass('erasing');
          return WritingInput[k] = ERASE;
        } else {
          KeyStateSpans[k].addClass('writing');
          KeyStateSpans[k].removeClass('erasing');
          return WritingInput[k] = WRITE;
        }
      } else {
        KeyStateSpans[k].removeClass('writing');
        KeyStateSpans[k].removeClass('erasing');
        return WritingInput[k] = 0;
      }
    });
  };

  inputStateToText = function(inputState) {
    var t;
    t = _.map(InputLetters, function(l) {
      var bit;
      bit = 1 << InputLetterBits.indexOf(l);
      if (0 < (inputState & bit)) {
        return l;
      } else {
        return '.';
      }
    });
    return t.join('');
  };

  Connection = (function() {

    function Connection() {
      this.send = __bind(this.send, this);

      this.openSocket = __bind(this.openSocket, this);
      this.openSocket();
      this.onmessage = function() {};
      this.opened = false;
    }

    Connection.prototype.openSocket = function() {
      var _this = this;
      this.ws = new WebSocket("ws://localhost:9999/service");
      this.ws.onopen = function() {
        console.log("socket opened");
        return _this.opened = true;
      };
      this.ws.onclose = function() {
        console.log("socket closed");
        _this.opened = false;
        return _this.openSocket();
      };
      this.ws.onerror = function(e) {
        return console.log("socket error", e);
      };
      return this.ws.onmessage = function(e) {
        return _this.onmessage(e);
      };
    };

    Connection.prototype.send = function(m) {
      var fr;
      if (!this.opened) {
        return;
      }
      this.ws.send(m);
      if (false) {
        console.log('ws.send', m);
        fr = new FileReader();
        fr.readAsText(m);
        return fr.onloadend = function() {
          return console.log(fr.result);
        };
      }
    };

    return Connection;

  })();

  conn = new Connection();

  conn.onmessage = function(e) {
    var fr, m, nextNodes, nextNodesDiv;
    if (typeof e.data === 'string') {
      if (e.data.match(/^bin/)) {
        return gNextBinaryMessageType = e.data;
      } else {
        try {
          m = JSON.parse(e.data);
        } catch (x) {
          console.log("unknown string:", e.data, x);
          return;
        }
        if (false) {

        } else if (m.t === 'ramHash') {
          $('#ramHash')[0].innerText = m.ramHash;
          $('#aStarH')[0].innerText = m.aStarH;
          if (OldRamHash !== m.ramHash) {
            $('#ramHash').addClass('changed');
          } else {
            $('#ramHash').removeClass('changed');
          }
          return OldRamHash = m.ramHash;
        } else if (m.t === 'aStarStatus') {
          console.log(m);
          return _.each(m.bestPath, function(inputState, i) {
            setFrameInput(AStarInitialized + i + 1, inputState, false);
            return frameChanged();
          });
        } else if (m.t === 'nextNodeInfos') {
          nextNodesDiv = $('#nextNodes')[0];
          nextNodesDiv.innerHTML = '';
          nextNodes = [];
          _.each(m.nextNodes, function(info, inputState) {
            info.inputState = parseInt(inputState);
            return nextNodes.push(info);
          });
          nextNodes.sort(function(a, b) {
            return a.aStarH - b.aStarH;
          });
          return _.each(nextNodes, function(node) {
            var div, inputText, span;
            inputText = inputStateToText(node.inputState);
            div = $("<div><span class='inputLink'>" + inputText + "</span>" + (sprintf("%.4f", node.aStarH)) + "</div>").appendTo(nextNodesDiv);
            span = $('.inputLink', div);
            return span.on('click', function() {
              console.log('asdf');
              setFrameInput(CurFrame + 1, node.inputState);
              return frameChanged(CurFrame + 1);
            });
          });
        } else {
          return console.log("unknown message:", m);
        }
      }
    } else {
      if (false) {

      } else if (gNextBinaryMessageType === 'binFrameInputs') {
        fr = new FileReader();
        fr.readAsArrayBuffer(e.data);
        return fr.onloadend = function() {
          FrameInputBytes = new Uint8Array(fr.result);
          frameCountChanged(FrameInputBytes.length);
          return frameChanged();
        };
      } else if (gNextBinaryMessageType === 'binFrameGfx') {
        fr = new FileReader();
        fr.readAsArrayBuffer(e.data);
        return fr.onloadend = function() {
          ScreenImageData.data.set(new Uint8Array(fr.result));
          return ScreenContext.putImageData(ScreenImageData, 0, 0);
        };
      } else if (gNextBinaryMessageType === 'binFrameRam') {
        fr = new FileReader();
        fr.readAsArrayBuffer(e.data);
        $RamSpans.removeClass('changed');
        return fr.onloadend = function() {
          var i, newRam, _i;
          newRam = new Uint8Array(fr.result);
          for (i = _i = 0; 0 <= 0x800 ? _i < 0x800 : _i > 0x800; i = 0 <= 0x800 ? ++_i : --_i) {
            if (OldRam[i] !== newRam[i]) {
              RamSpans[i].innerText = hexnum(newRam[i], 2);
              $(RamSpans[i]).addClass('changed');
            }
          }
          return OldRam = newRam;
        };
      } else if (gNextBinaryMessageType === 'binFrameLuaOutput') {
        fr = new FileReader();
        fr.readAsText(e.data);
        return fr.onloadend = function() {
          return $('#luaOutput')[0].innerText = fr.result;
        };
      } else if (gNextBinaryMessageType === 'binLuaSource') {
        fr = new FileReader();
        fr.readAsText(e.data);
        return fr.onloadend = function() {
          return $('#luaSource')[0].innerText = fr.result;
        };
      } else if (gNextBinaryMessageType === 'binRamBlackoutBytes') {
        fr = new FileReader();
        fr.readAsArrayBuffer(e.data);
        return fr.onloadend = function() {
          var array, blackoutBytes;
          array = new Uint16Array(fr.result);
          blackoutBytes = _.map(array, function(i) {
            return i;
          });
          return ramBlackoutBytesChanged(blackoutBytes);
        };
      } else {
        return console.log("unknown gNextBinaryMessageType: " + gNextBinaryMessageType);
      }
    }
  };

  $('#romFile').on('change', function(e) {
    var b, f;
    f = e.target.files[0];
    if (f) {
      b = new Blob(['openRom:', f]);
      conn.send(b);
      return frameChanged();
    }
  });

  $('#fm2File').on('change', function(e) {
    var f, fr;
    f = e.target.files[0];
    if (f) {
      fr = new FileReader();
      fr.readAsText(f);
      return fr.onloadend = function() {
        var frameInputs, frameNumbers, inputLines, lines, text;
        text = fr.result;
        lines = text.split("\n");
        inputLines = _.filter(lines, function(l) {
          return l.match(/^\|/);
        });
        frameCountChanged(inputLines.length);
        frameNumbers = $('#frameNumbers')[0];
        frameInputs = $('#frameInputs')[0];
        _.each(inputLines, function(inputLine, i) {
          var input, inputB;
          input = inputLine.match(/^...(.{8})/)[1];
          inputB = input;
          inputB = inputB.replace(/[^\.]/g, '1');
          inputB = inputB.replace(/\./g, '0');
          FrameInputBytes[i] = parseInt(inputB, 2);
          return FrameInputBytesChanged = true;
        });
        return frameChanged();
      };
    }
  });

  frameNumbers = $('#frameNumbers')[0];

  frameInputs = $('#frameInputs')[0];

  for (i = _i = 0; 0 <= FrameScrollerCount ? _i < FrameScrollerCount : _i > FrameScrollerCount; i = 0 <= FrameScrollerCount ? ++_i : --_i) {
    f = function(i) {
      var frameInputDiv, frameNumDiv, inputBits, j;
      frameNumDiv = $("<div>" + i + "</div>").appendTo(frameNumbers)[0];
      $(frameNumDiv).on('click', function(e) {
        var frameNum;
        frameNum = parseInt(frameNumDiv.innerHTML);
        return frameChanged(frameNum);
      });
      FrameScrollerNumbers.push(frameNumDiv);
      frameInputDiv = $("<div></div>").appendTo(frameInputs)[0];
      inputBits = (function() {
        var _j, _results;
        _results = [];
        for (j = _j = 0; _j < 8; j = ++_j) {
          _results.push($("<div>.</div>").appendTo(frameInputDiv)[0]);
        }
        return _results;
      })();
      FrameScrollerInputBits.push(inputBits);
      $(frameInputDiv).on('mousemove', function(e) {
        var n;
        n = i + CurFrame - FrameScrollerFocus;
        if (n < 0) {
          return;
        }
        return _.each(WritingInput, function(v, k) {
          var bit;
          bit = InputLetterBits.indexOf(k);
          if (v === WRITE) {
            return setFrameInput(n, (FrameInputBytes[n] | 0) | (1 << bit));
          } else if (v === ERASE) {
            return setFrameInput(n, (FrameInputBytes[n] | 0) & ~(1 << bit));
          }
        });
      });
      if (i === FrameScrollerFocus) {
        $(frameNumDiv).addClass('focus');
        return $(frameInputDiv).addClass('focus');
      }
    };
    f(i);
  }

  sendFrameCommandsTimeout = null;

  frameN = document.getElementById('frameN');

  frameChanged = function(v) {
    var sendFrameCommands;
    if (!(v != null)) {
      v = parseInt(frameN.value);
    }
    CurFrame = v | 0;
    if (CurFrame < 1) {
      CurFrame = 0;
    }
    frameN.value = '' + CurFrame;
    if (CurFrame > FrameCount) {
      frameCountInput.value = '' + CurFrame;
      frameCountChanged();
      return;
    }
    $('#timelineControls #timepos').css({
      left: (512 * (CurFrame / FrameCount)) + 'px'
    });
    _.each(FrameScrollerNumbers, function(frameNumDiv, i) {
      var n;
      n = i + CurFrame - FrameScrollerFocus;
      if (n < 0) {
        frameNumDiv.innerText = ' ';
        return _.each(FrameScrollerInputBits[i], function(div) {
          return div.innerText = ' ';
        });
      } else {
        frameNumDiv.innerText = '' + n;
        return _.each(FrameScrollerInputBits[i], function(div, b) {
          var bitOn;
          bitOn = 0 !== (FrameInputBytes[n] & (1 << b));
          return div.innerText = bitOn ? InputLetterBits[b] : '.';
        });
      }
    });
    clearTimeout(sendFrameCommandsTimeout);
    sendFrameCommands = function() {
      var blob;
      if (FrameInputBytesChanged) {
        FrameInputBytesChanged = false;
        conn.send(new Blob(['setFrameInputs:', FrameInputBytes]));
      }
      if (RamBlackoutBytesChanged) {
        RamBlackoutBytesChanged = false;
        blob = copyTypedArray(RamBlackoutBytes, Uint16Array, RamBlackoutBytes.length);
        conn.send(new Blob(['setRamBlackoutBytes:', blob]));
      }
      if (LuaSourceChanged) {
        LuaSourceChanged = false;
        conn.send(new Blob(['setLuaSource:', $('#luaSource').val()]));
      }
      return conn.send(new Blob(['getFrame:' + frameN.value]));
    };
    return sendFrameCommandsTimeout = setTimeout(sendFrameCommands, 100);
  };

  $(frameN).on('change', function() {
    return frameChanged();
  });

  frameCountInput = document.getElementById('frameCount');

  frameCountChanged = function(v) {
    if (!(v != null)) {
      v = parseInt(frameCountInput.value);
    }
    FrameCount = v | 0;
    if (FrameCount < 1) {
      FrameCount = 1;
    }
    FrameInputBytes = copyTypedArray(FrameInputBytes, Uint8Array, FrameCount);
    frameCountInput.value = '' + FrameCount;
    return frameChanged();
  };

  $(frameCountInput).on('change', frameCountChanged);

  $('#timelineControls #timebar').on('click', function(e) {
    var frame;
    frame = (e.offsetX / 512) * FrameCount;
    return frameChanged(frame);
  });

  $('#timelineControls input[type=button]').on('click', function(e) {
    var delta, frame;
    delta = parseInt(e.currentTarget.value);
    frame = parseInt(frameN.value);
    return frameChanged(frame + delta);
  });

  luaSourceChanged = function() {
    LuaSourceChanged = true;
    return frameChanged();
  };

  $('#luaSource').on('keyup', luaSourceChanged);

  $('#luaSource').on('change', luaSourceChanged);

  AStarInitialized = null;

  $('#cancelAStar').on('click', function() {
    AStarInitialized = null;
    return console.log('cancel a*');
  });

  $('#startAStar').on('click', function() {
    if (!AStarInitialized) {
      AStarInitialized = CurFrame;
      console.log('start a*');
      return conn.send(new Blob(['initAStar:' + CurFrame]));
    } else {
      console.log('step a*');
      return conn.send(new Blob(['workAStar:']));
    }
  });

  frameChanged();

}).call(this);
