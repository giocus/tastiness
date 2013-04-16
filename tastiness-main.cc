#include "tastiness.h"
#include "mongoose.git/mongoose.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

typedef uint8 InputState;

//timeline...
//gEmuInitialState + gInputTimeline[0] -> gEmuStateTimeline[0]
//gEmuStateTimeline[0] + gInputTimeline[1] -> gEmuStateTimeline[1]
vector<uint8> gEmuInitialState;
vector<vector<uint8> > gEmuStateTimeline;
vector<InputState> gInputTimeline;

vector<uint16> gRamBlackoutBytes;

string gLuaLoadError;
string gLuaOutput;
bool gLuaCodeIsValid = false;

lua_State* gLuaState;

struct mg_connection* gConn;

void websocket_ready_handler(struct mg_connection *conn);
void doStep(int frame);
void send_bin(mg_connection* conn, char* msgt, unsigned char* data, int data_len);

void cmd_openRom(mg_connection* conn, char* data, int data_len);
void cmd_getFrame(mg_connection* conn, char* data, int data_len);
void cmd_setFrameInputs(mg_connection* conn, char* data, int data_len);
void cmd_setRamBlackoutBytes(mg_connection* conn, char* data, int data_len);
void cmd_setLuaSource(mg_connection* conn, char* data, int data_len);

int websocket_data_handler(mg_connection *conn, int flags, char *data, size_t data_len) {
  char* colon = strnstr(data, ":", 64);
  if(colon == 0) {
      printf("ERROR: received invalid websocket frame (no command) - terminating connection\n");
      return 0;
  }
  char commandName[64];
  memcpy(commandName, data, colon-data);
  commandName[colon-data] = 0;

  char* commandData = colon+1;
  int commandDataLen = data_len - (commandData - data);

  printf("commandName: %s commandDataLen: %d\n", commandName, commandDataLen);

  uint64_t st = microTime();
  if(0);
#define CMD(n) else if(!strcmp(commandName, #n)) cmd_ ## n (conn, commandData, commandDataLen);
  CMD(openRom)
  CMD(getFrame)
  CMD(setFrameInputs)
  CMD(setRamBlackoutBytes)
  CMD(setLuaSource)
#undef CMD
  else {
      printf("ERROR: unhandled websocket command '%s' - terminating connection\n", commandName);
      return 0;
  }
  uint64_t et = microTime();
  printf("command took %.3fms\n", double(et-st)/1000);

  return 1;
}

void websocket_ready_handler(struct mg_connection *conn) {
  gConn = conn;
  printf("websocket client connected\n");
  send_bin(conn, "binInputs", &gInputTimeline[0], gInputTimeline.size());
}

static int l_my_print(lua_State* L) {
    int nargs = lua_gettop(L);

    string printstring;

    for (int i=1; i <= nargs; i++) {
      lua_getglobal(L, "tostring");
      lua_insert(L, -2);
      int error = lua_pcall(L, 1, 1, 0);
      string s;
      if(error) {
        s = string("<error invoking tostring: ") + lua_tostring(gLuaState, -1) + ">";
      } else {
        s = lua_tostring(L, -1);
      }
      lua_pop(L, 1);
      if(i>1) {
        printstring = string("\t") + printstring;
      }
      printstring = s + printstring;
    }
    gLuaOutput += printstring;
    gLuaOutput += '\n';

    return 0;
}

static int l_get_ram(lua_State* L) {
  double address = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  lua_pushnumber(L, RAM[((int)address) & 0xfff]);
  return 1;
}

static const struct luaL_reg mylib [] = {
  {"print", l_my_print},
  {"get_ram", l_get_ram},
  {NULL, NULL}
};

int main(int argc, char** argv) {
  const char* port = "9999";
  if(argc > 1 && atoi(argv[1])) {
    port = argv[1];
  }
  struct mg_context *ctx;
  struct mg_callbacks callbacks;
  const char *options[] = {
    "listening_ports", port,
    "document_root", "html_ui",
    NULL
  };

  gLuaState = lua_open();
  luaL_openlibs(gLuaState);

  lua_getglobal(gLuaState, "_G");
  luaL_register(gLuaState, NULL, mylib);
  lua_pop(gLuaState, 1);

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.websocket_ready = websocket_ready_handler;
  callbacks.websocket_data = websocket_data_handler;
  printf("\n\nTAStiNESs waiting for connection: http://localhost:%s/\n\n", port);
  ctx = mg_start(&callbacks, NULL, options);

  while(true) { sleep(60); }

  return 0;
}

void cmd_openRom(mg_connection* conn, char* data, int data_len) {
  static bool initialized = false;
  if(initialized) Emulator::Shutdown();
  initialized = true;

  char* romFilename = tempnam(0, "tastiness-rom-");
  printf("romFilename: %s length: %d\n", romFilename, data_len);
  FILE* fp = fopen(romFilename, "wb");
  fwrite(data, 1, data_len, fp);
  fclose(fp);
  Emulator::Initialize(romFilename);

  Emulator::SaveUncompressed(&gEmuInitialState);
  printf("Save states are %ld bytes.\n", gEmuInitialState.size());

  gEmuStateTimeline.clear();
}

struct BITMAPINFOHEADER {
  uint32 biSize;
  uint32 biWidth;
  uint32 biHeight;
  uint16 biPlanes;
  uint16 biBitCount;
  uint32 biCompression;
  uint32 biSizeImage;
  uint32 biXPelsPerMeter;
  uint32 biYPelsPerMeter;
  uint32 biClrUsed;
  uint32 biClrImportant;
};

void send_header(mg_connection* conn, bool binary, int data_len) {
  char header[64];
  char* c = header;
  if(binary) {
    *c++ = 0x82;
  } else {
    *c++ = 0x81;
  }
  if(data_len >= 65536) {
    *c++ = 0x7f; //size coming, 8 bytes
    ((uint32*)c)[0] = 0;
    c+=4;
    ((uint32*)c)[0] = htonl(data_len);
    c+=4;
  } else if(data_len >= 126) {
    *c++ = 0x7e; //size coming, 2 bytes
    ((uint16*)c)[0] = htons(data_len);
    c+=2;
  } else {
    *c++ = data_len;
  }
  mg_write(conn, header, c-header);
}

void send_txt(mg_connection* conn, char* msg) {
  int len = (int)strlen(msg);
  send_header(conn, false, len);
  mg_write(conn, msg, len);
}

void send_bin(mg_connection* conn, char* msgt, unsigned char* data, int data_len) {
  send_txt(conn, msgt);
  send_header(conn, true, data_len);
  mg_write(conn, data, data_len);
}


const int WIDTH = 256;
const int HEIGHT = 224;

uint8 getInputStateForFrame(int frame) {
  if(frame>=0 && frame < gInputTimeline.size()) {
    return gInputTimeline[frame];
  } else  {
    return 0;
  }
}

void cmd_setFrameInputs(mg_connection* conn, char* data, int data_len) {
  int invalidateFromIndex = gEmuStateTimeline.size();
  for(int i=0; i<data_len; i++) {
    uint8 input = ((uint8*)data)[i];
    if(i>=gInputTimeline.size() || gInputTimeline[i] != input) {
      invalidateFromIndex = i;
      break;
    }
  }
  gInputTimeline.resize(data_len);
  memcpy(&gInputTimeline[0], data, data_len);
  if(gEmuStateTimeline.size() > invalidateFromIndex) {
    printf("invalidating from index %d\n", invalidateFromIndex);
    gEmuStateTimeline.resize(invalidateFromIndex);
  }
  printf("gEmuStateTimeline.size(): %d\n", (int)gEmuStateTimeline.size());
}

void cmd_setRamBlackoutBytes(mg_connection* conn, char* data, int data_len) {
  gRamBlackoutBytes.resize(data_len/2);
  memcpy(&gRamBlackoutBytes[0], data, data_len);
  printf("gRamBlackoutBytes.size(): %lu\n", gRamBlackoutBytes.size());
  for(size_t i=0; i<gRamBlackoutBytes.size(); i++) {
    printf("gRamBlackoutBytes[%lu]: %d\n", i, gRamBlackoutBytes[i]);
  }
  gEmuStateTimeline.clear();
}

void cmd_setLuaSource(mg_connection* conn, char* data, int data_len) {
  int error = luaL_loadbuffer(gLuaState, data, data_len, "luaSource");
  if(error) {
    gLuaLoadError = string("Error loading lua code:\n")+ lua_tostring(gLuaState, -1);
    puts(gLuaLoadError.c_str());
    lua_pop(gLuaState, 1);
    gLuaCodeIsValid = false;
  } else {
    printf("loaded lua code\n");
    lua_setglobal(gLuaState, "LuaFrameFunc");
    gLuaLoadError = "";
    gLuaCodeIsValid = true;
  }
}

struct LuaScriptResult {
  string output;
  double aStarH;
};
void runLuaAgainstCurrentEmulatorState(LuaScriptResult& result) {
  result.aStarH = 1e6;
  if(!gLuaLoadError.empty()) {
    result.output = gLuaLoadError;
  } else if(gLuaCodeIsValid) {
    gLuaOutput.clear();
    lua_getglobal(gLuaState, "LuaFrameFunc");
    int error = lua_pcall(gLuaState, 0, 0, 0);
    if(error) {
      result.output = string("Error running lua code:\n") + lua_tostring(gLuaState, -1);
      lua_pop(gLuaState, 1);
    } else {
      result.output = gLuaOutput;
    }
  }
}

uint64 getRamHashForCurrentEmulatorState() {
  uint8 ramForHash[0x800];
  memcpy(ramForHash, RAM, 0x800);
  for(size_t i=0; i<gRamBlackoutBytes.size(); i++) {
    ramForHash[gRamBlackoutBytes[i]] = 0;
  }
  return CityHash64((const char*)ramForHash, 0x800);
}

struct NextNodeInfo {
  uint64 hash;
  double aStarH;
};

void getDistinctNextNodesFromCurrentEmulatorState(map<InputState, NextNodeInfo>& nextNodeInfos) {
  uint8* gfx;
  int32 *sound;
  int32 ssize;

  set<uint64> foundHashes;

  vector<uint8> thisState;
  Emulator::SaveUncompressed(&thisState);
  for(InputState inputState=0; inputState<0xff; inputState++) {
    Emulator::LoadUncompressed(&thisState);
    Emulator::Step(inputState, &gfx, &sound, &ssize, 2);
    uint64 hash = getRamHashForCurrentEmulatorState();
    if(foundHashes.count(hash) == 0) {
      foundHashes.insert(hash);
      nextNodeInfos[inputState].hash = hash;
      nextNodeInfos[inputState].aStarH = 0;
    }
  }
}

void cmd_getFrame(mg_connection* conn, char* data, int data_len) {
  if(gEmuInitialState.empty()) {
    return;
  }

  char buf[65536];
  strncpy(buf, data, data_len);
  buf[data_len]=0;
  int frame = atoi(buf);

  uint8* gfx = 0;
  int32 *sound = 0;
  int32 ssize = 0;

  //printf("getFrame() frame: %d gEmuStateTimeline.size(): %d\n", frame, gEmuStateTimeline.size());
  //step with frameskip as needed
  if(gEmuStateTimeline.size() <= frame) {
    if(gEmuStateTimeline.empty()) {
      Emulator::LoadUncompressed(&gEmuInitialState);
    } else {
      Emulator::LoadUncompressed(&gEmuStateTimeline[gEmuStateTimeline.size()-1]);
    }
    for(int f=gEmuStateTimeline.size(); f<frame; f++) {
      Emulator::Step(getInputStateForFrame(f), &gfx, &sound, &ssize, 2);
      gEmuStateTimeline.resize(f+1);
      Emulator::SaveUncompressed(&gEmuStateTimeline[f]);
      //printf("frameskip stepped into frame %d, input %02x, ram hash %016llx\n", f, getInputStateForFrame(f), CityHash64((const char*)RAM, 0x800));
    }
  }

  vector<uint8>& preState = (frame==0) ? gEmuInitialState : gEmuStateTimeline[frame-1];
  Emulator::LoadUncompressed(&preState);
  Emulator::Step(getInputStateForFrame(frame), &gfx, &sound, &ssize, 0);
  //printf("real stepped into frame %d, input %02x, ram hash %016llx\n", frame, getInputStateForFrame(frame), CityHash64((const char*)RAM, 0x800));

  send_bin(gConn, "binFrameRam", RAM, 0x800);

  sprintf(buf, "{\"t\":\"ramHash\", \"ramHash\":\"%016llx\"}", getRamHashForCurrentEmulatorState());
  send_txt(gConn, buf);

  static vector<uint32> palette;
  Emulator::GetPalette(palette);
  static uint32 raw[WIDTH*HEIGHT];
  for(int y=0; y<HEIGHT; y++) {
    for(int x=0; x<WIDTH; x++) {
      raw[y*WIDTH+x] = palette[gfx[y*WIDTH+x]];
    }
  }
  send_bin(gConn, "binFrameGfx", (uint8*)raw, WIDTH*HEIGHT*4);

  LuaScriptResult scriptResult;
  runLuaAgainstCurrentEmulatorState(scriptResult);
  send_bin(gConn, "binFrameLuaOutput", (uint8*)scriptResult.output.c_str(), scriptResult.output.size());

  map<InputState, NextNodeInfo> nextNodeInfos;
  getDistinctNextNodesFromCurrentEmulatorState(nextNodeInfos);
  char* c = buf;
  c += sprintf(c, "{\"t\":\"nextNodeInfos\"");
  map<InputState, NextNodeInfo>::const_iterator i = nextNodeInfos.begin();
  while(i != nextNodeInfos.end()) {
    c+= sprintf(c, ",\"%d\":{\"ramHash\":\"%016llx\",\"aStarH\":\"%f\"}", i->first, i->second.hash, i->second.aStarH);
    ++i;
  }
  c += sprintf(c, "}");
  send_txt(gConn, buf);
}
