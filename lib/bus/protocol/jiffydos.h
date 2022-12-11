// Meatloaf - A Commodore 64/128 multi-device emulator
// https://github.com/idolpx/meatloaf
// Copyright(C) 2020 James Johnston
//
// Meatloaf is free software : you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Meatloaf is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Meatloaf. If not, see <http://www.gnu.org/licenses/>.

// https://github.com/MEGA65/open-roms/blob/master/doc/Protocol-JiffyDOS.md
// http://www.nlq.de/
// http://www.baltissen.org/newhtm/sourcecodes.htm
// https://www.amigalove.com/viewtopic.php?t=1734


#ifndef PROTOCOL_JIFFYDOS_H
#define PROTOCOL_JIFFYDOS_H

#include "cbmstandardserial.h"

#define TIMING_JIFFY_BITPAIR
#define TIMING_JIFFY_BYTE

namespace Protocol
{
	class JiffyDOS : public CBMStandardSerial
	{
		protected:
			int16_t receiveByte(void) override;
			bool sendByte(uint8_t data, bool signalEOI) override;
	};
};

#endif // PROTOCOL_JIFFYDOS_H
