#include "tastiness.h"

const int WARMUP=100;
const int BENCHMARK=10000;

int main(int argc, char** argv) {
  Emulator::Initialize(argv[1]);

  if(true) {
    vector<uint8> save;
    Emulator::SaveUncompressed(&save);
    printf("Save states are %ld bytes.\n", save.size());

    if(true) {
      for(int i=0; i<WARMUP; i++) {
        Emulator::SaveUncompressed(&save);
      }
      uint64_t st = microTime();
      for(int i=0; i<BENCHMARK; i++) {
        Emulator::SaveUncompressed(&save);
      }
      uint64_t et = microTime();
      printf("%d saves took %lldusec:  %.1fusec each\n", BENCHMARK, et-st, double(et-st)/BENCHMARK);
    }

    if(true) {
      for(int i=0; i<WARMUP; i++) {
        Emulator::LoadUncompressed(&save);
      }
      uint64_t st = microTime();
      for(int i=0; i<BENCHMARK; i++) {
        Emulator::LoadUncompressed(&save);
      }
      uint64_t et = microTime();
      printf("%d loads took %lldusec:  %.1fusec each\n", BENCHMARK, et-st, double(et-st)/BENCHMARK);
    }

    if(true) {
      for(int i=0; i<WARMUP; i++) {
        Emulator::Step(0);
      }
      uint64_t st = microTime();
      for(int i=0; i<BENCHMARK; i++) {
        Emulator::Step(0);
      }
      uint64_t et = microTime();
      printf("%d steps took %lldusec:  %.1fusec each\n", BENCHMARK, et-st, double(et-st)/BENCHMARK);
    }

    if(true) {
      vector<uint8> save2;
      for(int i=0; i<WARMUP; i++) {
        Emulator::LoadUncompressed(&save);
        Emulator::Step(0);
        Emulator::SaveUncompressed(&save2);
      }
      uint64_t st = microTime();
      for(int i=0; i<BENCHMARK; i++) {
        Emulator::LoadUncompressed(&save);
        Emulator::Step(0);
        Emulator::SaveUncompressed(&save2);
      }
      uint64_t et = microTime();
      printf("%d load-step-saves took %lldusec:  %.1fusec each\n", BENCHMARK, et-st, double(et-st)/BENCHMARK);
    }
  }

  return 0;
}

