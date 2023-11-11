/*
# _____        ____   ___
#   |     \/   ____| |___|
#   |     |   |   \  |   |
#-----------------------------------------------------------------------
# Copyright 2022 - 2023, tyra - https://github.com/h4570/tyra
# Licensed under Apache License 2.0
# Wellington Carvalho <wellcoj@gmail.com>
# André Guilherme <andregui17@outlook.com>
*/

#include "irx/irx_loader.hpp"
#include "debug/debug.hpp"
#include <stdio.h>
#include <sys/stat.h>
#include <loadfile.h>
#include <kernel.h>
#include <sifrpc.h>
#include <sbv_patches.h>
#include <iopcontrol.h>
#include "file/file_utils.hpp"

// external IRX modules
#define EXTERN_IRX(_irx) \
  extern u8 _irx[]; \
  extern int size_##_irx

EXTERN_IRX(sio2man_irx);
EXTERN_IRX(padman_irx);
EXTERN_IRX(audsrv_irx);
EXTERN_IRX(libsd_irx);
EXTERN_IRX(fileXio_irx);
EXTERN_IRX(iomanX_irx);
EXTERN_IRX(bdm_irx);
EXTERN_IRX(bdmfs_fatfs_irx);
#ifdef USE_USBBDMINI
EXTERN_IRX(usbd_mini_irx);
EXTERN_IRX(usbmass_bd_irx);
#else
EXTERN_IRX(usbd_irx);
EXTERN_IRX(usbmass_bd_mini_irx);
#endif
EXTERN_IRX(ps2hdd_irx);
EXTERN_IRX(ps2fs_irx);
EXTERN_IRX(ps2dev9_irx);
EXTERN_IRX(ps2atad_irx);


namespace Tyra {

bool IrxLoader::isLoaded = false;

IrxLoader::IrxLoader() {
#ifdef RESET_IOP
  SifInitRpc(0);
#ifdef IOP
  while (!SifIopReset("", 0)) {
  };
#else
  while (!SifIopReset(nullptr, 0)) {
  };
#endif
  while (!SifIopSync()) {
  };

  SifInitRpc(0);
#endif
  this->applyRpcPatches();
}

IrxLoader::~IrxLoader() {}

void IrxLoader::loadAll(const bool& withUsb, const bool& withHdd,
                        const bool& isLoggingToFile) {
  if (isLoaded) {
    TYRA_LOG("IRX modules already loaded!");
    return;
  }

  loadIO(!isLoggingToFile);
  loadSio2man(!isLoggingToFile);
  loadPadman(!isLoggingToFile);
  loadLibsd(!isLoggingToFile);

  if (withUsb) {
    loadUsbModules(!isLoggingToFile);
  }

  if (withHdd) {
    loadHddModules(!isLoggingToFile);
  }

  loadAudsrv(true);

  isLoaded = true;
}

/**
 * @brief Apply the SBV LMB patch to allow modules to be loaded from a buffer in
 * EE RAM.
 *
 */
int IrxLoader::applyRpcPatches() {
  int ret;

  ret = sbv_patch_enable_lmb();
  TYRA_ASSERT(ret >= 0,
              "Failed to load Applying SBV Patches sbv_patch_enable_lmb");

  ret = sbv_patch_disable_prefix_check();
  TYRA_ASSERT(
      ret >= 0,
      "Failed to load Applying SBV Patches sbv_patch_disable_prefix_check");

  ret = sbv_patch_fileio();
  TYRA_ASSERT(ret >= 0, "Failed to load Applying SBV Patches sbv_patch_fileio");

  return ret;
}

void IrxLoader::loadLibsd(const bool& verbose) {
  if (verbose) TYRA_LOG("IRX: Loading libsd...");

  int ret;
  SifExecModuleBuffer(&libsd_irx, size_libsd_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: libsd_irx");

  if (verbose) TYRA_LOG("IRX: Libsd loaded!");
}

void IrxLoader::loadIO(const bool& verbose) {
  int ret;
  if (verbose) TYRA_LOG("IRX: Loading iomanX...");

  int ret;

  SifExecModuleBuffer(&iomanX_irx, size_iomanX_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: iomanX_irx");

  if (verbose) TYRA_LOG("IRX: iomanX loaded!");

  if (verbose) TYRA_LOG("IRX: Loading fileXio...");

  SifExecModuleBuffer(&fileXio_irx, size_fileXio_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: fileXio_irx");

  if (verbose) TYRA_LOG("IRX: fileXio_irx loaded!");
}

void IrxLoader::loadUsbModules(const bool& verbose) {
  if (verbose) TYRA_LOG("IRX: Loading usb modules...");

  int ret;
#ifdef USE_USBDMINI
  SifExecModuleBuffer(&usbd_mini_irx, size_usbd_mini_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: usbd_irx");

  SifExecModuleBuffer(&usbmass_bd_mini_irx, size_usbmass_bd_mini_irx, 0,
                      nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: usbmass");
#else
  SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: usbd_irx");

  SifExecModuleBuffer(&usbmass_bd_irx, size_usbmass_bd_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: usbmass");
#endif
  SifExecModuleBuffer(&bdm_irx, size_bdm_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: bdm_irx");

  SifExecModuleBuffer(&bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: bdmfs_fatfs");

  waitUntilUsbDeviceIsReady();

  if (verbose) TYRA_LOG("IRX: Usb modules loaded!");
}

void IrxLoader::loadHddModules(const bool& verbose) {
  if (verbose) TYRA_LOG("IRX: Loading Hdd Modules!");

  int ret;

  SifExecModuleBuffer(&ps2hdd_irx, size_ps2hdd_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: ps2hdd_irx");

  SifExecModuleBuffer(&ps2fs_irx, size_ps2fs_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: ps2fs_irx");

  SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: ps2dev9_irx");

#ifdef ATAD //PS2 Atad support
  SifExecModuleBuffer(&ps2atad_irx, size_ps2atad_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: ps2atad.irx");
#endif
  if (verbose) TYRA_LOG("IRX: Hdd modules loaded");
}

void IrxLoader::loadAudsrv(const bool& verbose) {
  if (verbose) TYRA_LOG("IRX: Loading audsrv...");

  int ret;
  SifExecModuleBuffer(&audsrv_irx, size_audsrv_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: audsrv_irx");

  if (verbose) TYRA_LOG("IRX: Audsrv loaded!");
}

void IrxLoader::loadSio2man(const bool& verbose) {
  if (verbose) TYRA_LOG("IRX: Loading sio2man...");

  int ret;
  SifExecModuleBuffer(&sio2man_irx, size_sio2man_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: sio2man_irx");

  if (verbose) TYRA_LOG("IRX: Sio2man loaded!");
}

void IrxLoader::loadPadman(const bool& verbose) {
  if (verbose) TYRA_LOG("IRX: Loading padman...");

  int ret;
  SifExecModuleBuffer(&padman_irx, size_padman_irx, 0, nullptr, &ret);
  TYRA_ASSERT(ret >= 0, "Failed to load module: padman_irx");

  if (verbose) TYRA_LOG("IRX: Padman loaded!");
}

void IrxLoader::delay(int count) {
  int i;
  int ret;
  for (i = 0; i < count; i++) {
    ret = 0x01000000;
    while (ret--) asm("nop\nnop\nnop\nnop");
  }
}

void IrxLoader::waitUntilUsbDeviceIsReady() {
  struct stat buffer;
  int ret = -1;
  int retries = 50;

  delay(5);  // some delay is required by usb mass storage driver

  while (ret != 0 && retries > 0) {
    ret = stat("mass:/", &buffer);
    /* Wait until the device is ready */
    nopdelay();

    retries--;
  }
}

}  // namespace Tyra
