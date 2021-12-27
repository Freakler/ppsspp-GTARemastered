/*
  Remastered Controls: Grand Theft Auto
  Copyright (C) 2018, TheFloW [Original Author] (https://github.com/TheOfficialFloW/RemasteredControls/blob/master/GrandTheftAuto/main.c)
  Copyright (C) 2020, Unknown W. Brackets (https://github.com/hrydgard/ppsspp/pull/13335)
  Copyright (C) 2021, Freakler
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>

#include <stdio.h>
#include <string.h>

#include <systemctrl.h>


#define EMULATOR_DEVCTL__IS_EMULATOR 0x00000003

#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);

PSP_MODULE_INFO("GTARemasteredPPSSPP", 0, 1, 0);



enum GtaPad {
  PAD_LX = 1,
  PAD_LY = 2,
  PAD_RX = 3,
  PAD_RY = 4,
};

short cameraX(short *pad) {
  return pad[PAD_RX];
}

short cameraY(short *pad) {
  return pad[PAD_RY];
}

short aimX(short *pad) {
  return pad[PAD_LX] ? pad[PAD_LX] : pad[PAD_RX];
}

short aimY(short *pad) {
  return pad[PAD_LY] ? pad[PAD_LY] : pad[PAD_RY];
}

static int PatchLCS(u32 addr, u32 text_addr) { // Liberty City Stories
  // Implement right analog stick
  if (_lw(addr + 0x00) == 0x10000006 && _lw(addr + 0x04) == 0xA3A70013) {
    _sw(0xA7A50000 | (_lh(addr + 0x1C)), addr + 0x24);      // sh $a1, X($sp)
    _sw(0x97A50000 | (_lh(addr - 0xC) + 0x2), addr + 0x1C); // lhu $a1, X($sp)
    return 1;
  }

  // Redirect camera movement
  if (_lw(addr + 0x00) == 0x14800034 && _lw(addr + 0x10) == 0x10400014) {
    _sw(0x00000000, addr + 0x00);
    _sw(0x10000014, addr + 0x10);
    MAKE_CALL(addr + 0x84, cameraX);
    _sw(0x00000000, addr + 0x100);
    _sw(0x10000002, addr + 0x110);
    MAKE_CALL(addr + 0x13C, cameraY);
    return 1;
  }

  // Redirect gun aim movement
  if (_lw(addr + 0x00) == 0x04800036 && _lw(addr + 0x08) == 0x10800034) {
    MAKE_CALL(addr + 0x3C, aimX);
    MAKE_CALL(addr + 0x68, aimX);
    MAKE_CALL(addr + 0x78, aimX);
    MAKE_CALL(addr + 0x130, aimY);
    MAKE_CALL(addr + 0x198, aimY);
    return 1;
  }

  // Allow using L trigger when walking
  if (_lw(addr + 0x00) == 0x14A0000E && _lw(addr + 0x10) == 0x10A00008 && _lw(addr + 0x1C) == 0x04A00003) {
    _sw(0, addr + 0x10);
    _sw(0, addr + 0x74);
    return 1;
  }

  // Force L trigger value in the L+camera movement function
  if (_lw(addr + 0x00) == 0x850A000A) {
    _sw(0x240AFFFF, addr + 0x00);
    return 1;
  }

  return 0;
}


static int PatchVCS(u32 addr, u32 text_addr) { // Vice City Stories
  // Implement right analog stick
  if (_lw(addr + 0x00) == 0x10000006 && _lw(addr + 0x04) == 0xA3A70003) {
    _sw(0xA7A50000 | (_lh(addr + 0x1C)), addr + 0x24);      // sh $a1, X($sp)
    _sw(0x97A50000 | (_lh(addr - 0xC) + 0x2), addr + 0x1C); // lhu $a1, X($sp)
    return 1;
  }

  // Redirect camera movement
  if (_lw(addr + 0x00) == 0x14800036 && _lw(addr + 0x10) == 0x10400016) {
    _sw(0x00000000, addr + 0x00);
    _sw(0x10000016, addr + 0x10);
    MAKE_CALL(addr + 0x8C, cameraX);
    _sw(0x00000000, addr + 0x108);
    _sw(0x10000002, addr + 0x118);
    MAKE_CALL(addr + 0x144, cameraY);
    return 1;
  }

  // Redirect gun aim movement
  if (_lw(addr + 0x00) == 0x04800040 && _lw(addr + 0x08) == 0x1080003E) {
    MAKE_CALL(addr + 0x50, aimX);
    MAKE_CALL(addr + 0x7C, aimX);
    MAKE_CALL(addr + 0x8C, aimX);
    MAKE_CALL(addr + 0x158, aimY);
    MAKE_CALL(addr + 0x1BC, aimY);
    return 1;
  }

  // Allow using L trigger when walking
  if (_lw(addr + 0x00) == 0x1480000E && _lw(addr + 0x10) == 0x10800008 && _lw(addr + 0x1C) == 0x04800003) {
    _sw(0, addr + 0x10);
    _sw(0, addr + 0x9C);
    return 1;
  }

  // Force L trigger value in the L+camera movement function
  if (_lw(addr + 0x00) == 0x84C7000A) {
    _sw(0x2407FFFF, addr + 0x00);
    return 1;
  }

  return 0;	
}


static void CheckModules() { 
  SceUID modules[10];
  SceKernelModuleInfo info;
  int i, j, gta_version = -1, count = 0;
  
  if (sceKernelGetModuleIdList(modules, sizeof(modules), &count) >= 0) {
    for (i = 0; i < count; ++i) {
      info.size = sizeof(SceKernelModuleInfo);
      
	  if (sceKernelQueryModuleInfo(modules[i], &info) < 0) {
        continue;
      }
	  
      if (strcmp(info.name, "GTA3") == 0) { // GTA City Stories found
		u32 text_addr = info.text_addr;
		u32 text_size = info.text_size;
		
		for (j = 0; j < text_size; j += 4) {
		  u32 addr = text_addr + j;

		  if ((gta_version == -1 || gta_version == 0) && PatchVCS(addr, text_addr)) {
			gta_version = 0; // VCS found
			continue;
		  }

		  if ((gta_version == -1 || gta_version == 1) && PatchLCS(addr, text_addr)) {
			gta_version = 1; // LCS found
			continue;
		  }
		}

		sceKernelDcacheWritebackAll();
		return;
      }
    }
  }
}


int module_start(SceSize argc, void* argp) {
  if(sceIoDevctl("kemulator:", EMULATOR_DEVCTL__IS_EMULATOR, NULL, 0, NULL, 0) == 0) { // PPSSPP check
    CheckModules();
  } 

  return 0;
}