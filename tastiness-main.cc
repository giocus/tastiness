#include "tastiness.h"
#include "mongoose.git/mongoose.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

//timeline...
//gEmuStates[0] + gJoyStates[1] -> gEmuStates[1]
vector<uint8> gEmuInitialState;
vector<vector<uint8> > gEmuStates;
vector<uint8> gJoyStates;

string gLuaLoadError;
string gLuaOutput;
bool gLuaCodeIsValid = false;

lua_State* gLuaState;
int gLuaFrame;

struct mg_connection* gConn;

void websocket_ready_handler(struct mg_connection *conn);
void doStep(int frame);
void send_bin(mg_connection* conn, char* msgt, unsigned char* data, int data_len);

void cmd_openRom(mg_connection* conn, char* data, int data_len);
void cmd_getFrame(mg_connection* conn, char* data, int data_len);
void cmd_setFrameInputs(mg_connection* conn, char* data, int data_len);
void cmd_setLuaSource(mg_connection* conn, char* data, int data_len);

int websocket_data_handler(mg_connection *conn, int flags, char *data, size_t data_len) {
  char* colon = strnstr(data, ":", 16);
  if(colon == 0) {
      printf("ERROR: received invalid websocket frame (no command) - terminating connection\n");
      return 0;
  }
  char commandName[16];
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
  send_bin(conn, "binInputs", &gJoyStates[0], gJoyStates.size());
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

static int l_get_frame(lua_State* L) {
  lua_pushnumber(L, gLuaFrame);
  return 1;
}

static const struct luaL_reg mylib [] = {
  {"print", l_my_print},
  {"get_ram", l_get_ram},
  {"get_frame", l_get_frame},
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

  gEmuStates.clear();
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

uint8 getJoyStateForFrame(int frame) {
  if(frame>=0 && frame < gJoyStates.size()) {
    return gJoyStates[frame];
  } else  {
    return 0;
  }
}

void cmd_setFrameInputs(mg_connection* conn, char* data, int data_len) {
  int invalidateFromIndex = gEmuStates.size();
  for(int i=0; i<data_len; i++) {
    uint8 input = ((uint8*)data)[i];
    if(i>=gJoyStates.size() || gJoyStates[i] != input) {
      invalidateFromIndex = i;
      break;
    }
  }
  gJoyStates.resize(data_len);
  memcpy(&gJoyStates[0], data, data_len);
  if(gEmuStates.size() > invalidateFromIndex) {
    printf("invalidating from index %d\n", invalidateFromIndex);
    gEmuStates.resize(invalidateFromIndex);
  }
  printf("gEmuStates.size(): %d\n", (int)gEmuStates.size());
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

void cmd_getFrame(mg_connection* conn, char* data, int data_len) {
  if(gEmuInitialState.empty()) {
    return;
  }

  char buf[32];
  strncpy(buf, data, data_len);
  buf[data_len]=0;
  int frame = atoi(buf);

  uint8* gfx = 0;
  int32 *sound = 0;
  int32 ssize = 0;

  //printf("getFrame() frame: %d gEmuStates.size(): %d\n", frame, gEmuStates.size());
  //step with frameskip as needed
  if(gEmuStates.size() <= frame) {
    if(gEmuStates.empty()) {
      Emulator::LoadUncompressed(&gEmuInitialState);
    } else {
      Emulator::LoadUncompressed(&gEmuStates[gEmuStates.size()-1]);
    }
    for(int f=gEmuStates.size(); f<frame; f++) {
      Emulator::Step(getJoyStateForFrame(f), &gfx, &sound, &ssize, 2);
      gEmuStates.resize(f+1);
      Emulator::SaveUncompressed(&gEmuStates[f]);
      //printf("frameskip stepped into frame %d, input %02x, ram hash %016llx\n", f, getJoyStateForFrame(f), CityHash64((const char*)RAM, 0x800));
    }
  }

  vector<uint8>& preState = (frame==0) ? gEmuInitialState : gEmuStates[frame-1];
  Emulator::LoadUncompressed(&preState);
  Emulator::Step(getJoyStateForFrame(frame), &gfx, &sound, &ssize, 0);
  //printf("real stepped into frame %d, input %02x, ram hash %016llx\n", frame, getJoyStateForFrame(frame), CityHash64((const char*)RAM, 0x800));

  send_bin(gConn, "binFrameRam", RAM, 0x800);

  static vector<uint32> palette;
  Emulator::GetPalette(palette);
  static uint32 raw[WIDTH*HEIGHT];
  for(int y=0; y<HEIGHT; y++) {
    for(int x=0; x<WIDTH; x++) {
      raw[y*WIDTH+x] = palette[gfx[y*WIDTH+x]];
    }
  }
  send_bin(gConn, "binFrameGfx", (uint8*)raw, WIDTH*HEIGHT*4);

  gLuaFrame = frame;
  const char* luaOutput = "";
  gLuaOutput.clear();
  if(!gLuaLoadError.empty()) {
    send_bin(gConn, "binFrameLuaOutput", (uint8*)gLuaLoadError.c_str(), gLuaLoadError.size());
  } else if(gLuaCodeIsValid) {
    lua_getglobal(gLuaState, "LuaFrameFunc");
    int error = lua_pcall(gLuaState, 0, 0, 0);
    //printf("called LuaFrameFunc, stack: %d, error: %d\n", lua_gettop(gLuaState), error);
    if(error) {
      string s = string("Error running lua code:\n") + lua_tostring(gLuaState, -1);
      puts(s.c_str());
      lua_pop(gLuaState, 1);
      send_bin(gConn, "binFrameLuaOutput", (uint8*)s.c_str(), s.size());
    } else {
      send_bin(gConn, "binFrameLuaOutput", (uint8*)gLuaOutput.c_str(), gLuaOutput.size());
    }
  }
}

