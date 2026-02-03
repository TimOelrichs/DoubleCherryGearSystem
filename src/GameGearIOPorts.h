/*
 * Gearsystem - Sega Master System / Game Gear Emulator
 * Copyright (C) 2013  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#ifndef GAMEGEARIOPORTS_H
#define	GAMEGEARIOPORTS_H

#include "IOPorts.h"

class Audio;
class Video;
class Input;
class Cartridge;
class Memory;


class GameGearIOPorts : public IOPorts
{
public:
    GameGearIOPorts(Audio* pAudio, Video* pVideo, Input* pInput, Cartridge* pCartridge, Memory* pMemory);
    virtual ~GameGearIOPorts();
    void Reset();
    virtual u8 DoInput(u8 port);
    virtual void DoOutput(u8 port, u8 value);
    virtual void SaveState(std::ostream& stream);
    virtual void LoadState(std::istream& stream);
    void SetBaudrate(u8 selection);
    int GetBaudrate() { return m_current_serial_baudrate; };
    void setLinkedPorts(GameGearIOPorts* other);

    void UpdateSerial(int clocks);
	void UpdateParallelTransfers();

    void receiveSerial(u8 data);
private:
    GameGearIOPorts* m_linkedGameGear;
    Audio* m_pAudio;
    Video* m_pVideo;
    Input* m_pInput;
    Memory* m_pMemory;
    Cartridge* m_pCartridge;

    u8 m_Port3F;
    u8 m_Port3F_HC;

  int m_serial_baurate_options[4] = { 4800,2400,1200,300 };
    int m_current_serial_baudrate = 4800;
    int m_clocks_until_transfer_complete = 0;
    bool m_serial_transfer_active = false;
  u8  m_serialSendData = 0x00;

	// Serial Mode Methods
    int calculateTransferClocks() ;
    void sendSerial(u8 data);
    bool activeTransfer();
    void StartTransfer();




    bool isSerialModeEnabled();
    bool isReceiveInterruptEnabled();
    bool isReceiveEnabled() const;

	bool hasTriggeredParallelNMI(u8 oldValue, u8 newValue);

    // Bit Scrambling for Parallel Mode
    u8 scrambleParallelBits(u8 data) const;
    u8 unscrambleParallelBits(u8 data) const;
    bool isOutputBit(u8 bit);

    bool isTransmitEnabled() const;
    bool isNINTEnabled() const;  // Für Parallel NMI

};

#endif	/* GAMEGEARIOPORTS_H */
