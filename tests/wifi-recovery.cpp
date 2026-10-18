#include <cassert>
#include <cstdint>
#include "../firmware/include/WifiRecovery.h"

int main() {
  WifiRecovery recovery;
  assert(!recovery.update(false, 100, 60000));
  assert(!recovery.update(false, 15100, 60000));
  assert(!recovery.update(false, 60099, 60000));
  assert(recovery.update(false, 60100, 60000));
  assert(!recovery.update(true, 70000, 60000));
  assert(!recovery.update(false, 80000, 60000));
  assert(!recovery.update(false, 139999, 60000));
  assert(recovery.update(false, 140000, 60000));
  WifiRecovery rollover;
  assert(!rollover.update(false, UINT32_MAX - 30000, 60000));
  assert(!rollover.update(false, 10000, 60000));
  assert(rollover.update(false, 30000, 60000));
}
