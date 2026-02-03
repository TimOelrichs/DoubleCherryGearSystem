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

#include "GameGearIOPorts.h"
#include "Audio.h"
#include "Video.h"
#include "Input.h"
#include "Cartridge.h"
#include "Memory.h"
#include "Processor.h"
#include "../platforms/libretro/libretro.h"

extern retro_log_printf_t log_cb;

GameGearIOPorts::GameGearIOPorts(Audio* pAudio, Video* pVideo, Input* pInput, Cartridge* pCartridge, Memory* pMemory)
{
    m_pAudio = pAudio;
    m_pVideo = pVideo;
    m_pInput = pInput;
    m_pCartridge = pCartridge;
    m_pMemory = pMemory;
    Reset();
}

GameGearIOPorts::~GameGearIOPorts()
{
}

inline bool getBit(u8 value, u8 bit) {
    return (value >> bit) & 1;
}

inline void updateBit(u8& value, u8 bit, bool setToTrue) {
    if (setToTrue) {
        value |= (1u << bit);    // Bit auf 1 setzen
    } else {
        value &= ~(1u << bit);   // Bit auf 0 setzen
    }
}


void GameGearIOPorts::Reset()
{

    m_Port3F = 0;
    m_Port3F_HC = 0;
    m_Port0 = 0xC0;
    m_Port1 = 0x7F;
    m_Port2 = 0xFF;
    m_Port3 = 0x00;
    m_Port4 = 0xFF;
    m_Port5 = 0xF8;
    m_Port6 = 0xFF;
    m_clocks_until_transfer_complete = 0;
    m_serial_transfer_active = false;
}

bool GameGearIOPorts::activeTransfer()
{
    return (m_Port5 & 0x01) != 0; // Bit 0 = TXFL
}

void GameGearIOPorts::StartTransfer()
{
    if(m_serial_transfer_active) return;
    // Timer basierend auf Baudrate berechnen
    m_clocks_until_transfer_complete = calculateTransferClocks();
    m_serial_transfer_active = true;
    m_Port5 |= 0x01;
    log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Start Transfer\n");

}

void GameGearIOPorts::setLinkedPorts(GameGearIOPorts* other)
{
    if (other == this)
    {
        // Verhindere Selbst-Referenz
        log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Cannot link to itself");

        return;
    }

    m_linkedGameGear = other;

    // Optional: Verifikation hinzufügen
    if (m_linkedGameGear)
    {
        log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Linked to another instance\n");

    }
    else
    {
        log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Link removed\n");
    }
}
void GameGearIOPorts::UpdateSerial(int clocks)
{
    if (!m_serial_transfer_active) return;

    if (m_clocks_until_transfer_complete > 0)
    {
        m_clocks_until_transfer_complete -= clocks;

        if (m_clocks_until_transfer_complete <= 0)
        {
            // Übertragung abgeschlossen
            sendSerial(m_Port3);
        }
    }

};
void GameGearIOPorts::sendSerial(u8 data) {
    // TXFL löschen (Bit 0)
    m_Port5 &= ~0x01;
    m_serial_transfer_active = false;
    log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Send %u\n",data);

    // Daten an verbundene Instanz senden
    if (m_linkedGameGear)
    {
        m_linkedGameGear->receiveSerial(m_serialSendData);
    }
};

void GameGearIOPorts::receiveSerial(u8 data)
{
    // Prüfe ob Empfang aktiviert ist (RON Bit 5)
    if (!isReceiveEnabled())
    {
        log_cb(RETRO_LOG_WARN, "GameGearIOPorts: Received but RON not set\n");
        return;
    }

    // Daten in Empfangspuffer schreiben
    m_Port4 = data;

    // RXRD setzen (Bit 1)
    m_Port5 |= 0x02;

    log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Received 0x%02X, Port5 now 0x%02X\n",
           data, m_Port5);

    // NMI auslösen (wenn Interrupt enabled)
    if (isReceiveInterruptEnabled())
    {
        Processor* processor = m_pMemory->GetProcessor();
        if (processor)
        {
            processor->RequestNMI();
            log_cb(RETRO_LOG_INFO, "GameGearIOPorts: NMI requested\n");
        }
        else
        {
            log_cb(RETRO_LOG_WARN, "GameGearIOPorts: No processor for NMI\n");
        }
    }
    else
    {
        log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Interrupt not enabled (Port5=0x%02X)\n", m_Port5);
    }
}

void GameGearIOPorts::SetBaudrate(u8 selection)
{
m_current_serial_baudrate = m_serial_baurate_options[selection];
    log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Set Baudrate %u\n",m_serial_baurate_options[selection]);
};

int GameGearIOPorts::calculateTransferClocks()
{
    // Game Gear CPU Clock: 3.579545 MHz (NTSC)
    const double CPU_CLOCK_NTSC = 3579545.0;  // 3.579545 MHz

    // Tatsächlicher Teiler-Wert aus der Hardware
    // Diese Werte basieren auf den Baudraten-Optionen
    double clocks_per_bit = 0;

    switch (m_current_serial_baudrate)
    {
    case 4800:
        clocks_per_bit = CPU_CLOCK_NTSC / 4800.0;
        break;
    case 2400:
        clocks_per_bit = CPU_CLOCK_NTSC / 2400.0;
        break;
    case 1200:
        clocks_per_bit = CPU_CLOCK_NTSC / 1200.0;
        break;
    case 300:
        clocks_per_bit = CPU_CLOCK_NTSC / 300.0;
        break;
    default:
        clocks_per_bit = CPU_CLOCK_NTSC / 4800.0;  // Default
    }

    int total_clocks = static_cast<int>(clocks_per_bit * 8);

    log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Transfer clocks: %d (baud=%d, CPU=%.1f MHz)\n",
           total_clocks, m_current_serial_baudrate, CPU_CLOCK_NTSC / 1000000.0);

    return total_clocks;
}

u8 GameGearIOPorts::DoInput(u8 port)
{
    if (port < 0x07)
    {
        switch (port)
        {
            case 0x00:
            {
                u8 port00 = m_pInput->GetPort00();
                if (m_pCartridge->GetZone() != Cartridge::CartridgeJapanGG)
                    port00 |= 0x40;
                return port00;
            }
            case 0x01:
            {
                log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Read Port 1:  %u\n",m_Port1);

                if(!m_linkedGameGear) return m_Port1 & 0x7F;
                u8 result = m_Port1;
                if(m_linkedGameGear->isOutputBit(2)) updateBit(result, 0, getBit(m_linkedGameGear->m_Port1, 2));
                if(m_linkedGameGear->isOutputBit(3)) updateBit(result, 1, getBit(m_linkedGameGear->m_Port1, 3));
                if(m_linkedGameGear->isOutputBit(0)) updateBit(result, 2, getBit(m_linkedGameGear->m_Port1, 0));
                if(m_linkedGameGear->isOutputBit(1)) updateBit(result, 3, getBit(m_linkedGameGear->m_Port1, 1));
                if(m_linkedGameGear->isOutputBit(5)) updateBit(result, 4, getBit(m_linkedGameGear->m_Port1, 5));
                if(m_linkedGameGear->isOutputBit(4)) updateBit(result, 5, getBit(m_linkedGameGear->m_Port1, 4));
                if(m_linkedGameGear->isOutputBit(6)) updateBit(result, 6, getBit(m_linkedGameGear->m_Port1, 6));

                return result & 0x7F;
            }
            case 0x02:
                log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Read Port 2:  %u\n",m_Port2);
                return m_Port2;
            case 0x03:
                return m_Port3;
            case 0x04:
              {  // Beim Lesen von Port $04: RXRD zurücksetzen
                u8 data = m_Port4;
                m_Port5 &= ~0x02; // Bit 1 (RXRD) löschen
                log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Read Port 4:  %u\n",data);
                return data;
            }
            case 0x05:
                return m_Port5;
            default:
                return 0xFF;
        }
    }
    else if (port < 0x40)
    {
        // Reads return $FF (GG)
        Log("--> ** Attempting to read from port $%X", port);
        return 0xFF;
    }
    else if ((port >= 0x40) && (port < 0x80))
    {

        // Reads from even addresses return the V counter
        // Reads from odd addresses return the H counter
        if ((port & 0x01) == 0x00)
            return m_pVideo->GetVCounter();
        else
            return m_pVideo->GetHCounter();
    }
    else if ((port >= 0x80) && (port < 0xC0))
    {
        // Reads from even addresses return the VDP data port contents
        // Reads from odd address return the VDP status flags
        if ((port & 0x01) == 0x00)
            return m_pVideo->GetDataPort();
        else
            return m_pVideo->GetStatusFlags();
    }
    else
    {
        // Reads from $C0 and $DC return the I/O port A/B register.
        // Reads from $C1 and $DD return the I/O port B/misc. register.
        // The remaining locations return $FF.
        switch (port)
        {
            case 0xC0:
            case 0xDC:
            {
                return m_pInput->GetPortDC();
            }
            case 0xC1:
            case 0xDD:
            {
                return ((m_pInput->GetPortDD() & 0x3F) | (m_Port3F & 0xC0));
            }
            default:
            {
                Log("--> ** Attempting to read from port $%X", port);
                return 0xFF;
            }
        }
    }
}

void GameGearIOPorts::DoOutput(u8 port, u8 value)
{
    if (port < 0x07)
    {

        switch (port)
        {
        case 0x00: m_Port0 = value; break; 
        case 0x01: m_Port1 = value; break; 
        case 0x02: 
        {

             log_cb(RETRO_LOG_INFO, "GameGearIOPorts: Wrote Port 2:  %u\n",value);
            if (!(m_Port2 & 0x80)) // If NINT Gen enabled
            {
                if (m_Port2 & 0x40 && !(value & 0x40)) // if D6 is Input and falls
                {
                    log_cb(RETRO_LOG_INFO, "Triggered ParallelMode NMI:  %u\n",value);
                    m_pMemory->GetProcessor()->RequestNMI();
                }

            }
                m_Port2 = value;
             break;

        }

        case 0x03:
            m_Port3 = value;
            m_serialSendData = value;

            if (activeTransfer())
            {
                log_cb(RETRO_LOG_WARN, "GameGearIOPorts: Port 3 write ignored, transfer active\n");
                return;
            }
            if (isSerialModeEnabled())
            {
                StartTransfer();
            }
            else
            {
                log_cb(RETRO_LOG_DEBUG, "GameGearIOPorts: Port 3 written but serial mode off\n");
            }
            break;
        case 0x04:
        {
    /* read only
            m_Port4 = value;  //The receive data is set during serial communications.
            m_Port5 |= 0x02; 
            m_pMemory->GetProcessor()->RequestNMI();
    */
            break;
        }
        case 0x05: 
        {
                // Control-Bits (3-7) schreiben, Status-Bits (0-2) bleiben
                u8 old_value = m_Port5;
                u8 control_mask = 0xF8; // Bits 3-7

                m_Port5 = (old_value & ~control_mask) | (value & control_mask);
                SetBaudrate((value >> 6) & 0x03);
                break;
        }
        case 0x06: m_pAudio->WriteGGStereoRegister(value); break; // SN76489 PSG
        }

    }
    else if (port < 0x40)
    {
        // Writes to even addresses go to memory control register.
        // Writes to odd addresses go to I/O control register.
        if ((port & 0x01) == 0x00)
        {
            Log("--> ** Output to memory control port $%X: %X", port, value);
            m_pMemory->SetPort3E(value);
        }
        else
        {
            if (((value  & 0x01) && !(m_Port3F_HC & 0x01)) || ((value  & 0x08) && !(m_Port3F_HC & 0x08)))
                m_pVideo->LatchHCounter();
            m_Port3F_HC = value & 0x05;

            m_Port3F =  ((value & 0x80) | (value & 0x20) << 1) & 0xC0;
            if (m_pCartridge->GetZone() == Cartridge::CartridgeJapanGG)
                m_Port3F ^= 0xC0;
        }
    }
    else if ((port >= 0x40) && (port < 0x80))
    {
        // Writes to any address go to the SN76489 PSG
        m_pAudio->WriteAudioRegister(value);
    }
    else if ((port >= 0x80) && (port < 0xC0))
    {
        // Writes to even addresses go to the VDP data port.
        // Writes to odd addresses go to the VDP control port.
        if ((port & 0x01) == 0x00)
            m_pVideo->WriteData(value);
        else
            m_pVideo->WriteControl(value);
    }
#ifdef DEBUG_GEARSYSTEM
    else
    {
        // Writes have no effect.
        if ((port == 0xDE) || (port == 0xDF))
        {
            Log("--> ** Output to keyboard port $%X: %X", port, value);
        }
        else if ((port == 0xF0) || (port == 0xF1) || (port == 0xF2))
        {
            Log("--> ** Output to YM2413 port $%X: %X", port, value);
        }
        else
        {
            Log("--> ** Output to port $%X: %X", port, value);
        }
    }
#endif
}

void GameGearIOPorts::SaveState(std::ostream& stream)
{
    using namespace std;

    stream.write(reinterpret_cast<const char*> (&m_Port3F), sizeof(m_Port3F));
    stream.write(reinterpret_cast<const char*> (&m_Port3F_HC), sizeof(m_Port3F_HC));
}

void GameGearIOPorts::LoadState(std::istream& stream)
{
    using namespace std;

    stream.read(reinterpret_cast<char*> (&m_Port3F), sizeof(m_Port3F));
    stream.read(reinterpret_cast<char*> (&m_Port3F_HC), sizeof(m_Port3F_HC));
}


bool GameGearIOPorts::isSerialModeEnabled()
{
    // Serial Mode active when TON (Bit 4) or RON (Bit 5) are set
    return (m_Port5 & 0x30) != 0;
}

bool GameGearIOPorts::isReceiveInterruptEnabled()
{
    // INT (Bit 3) for Receive-Interrupt
    return (m_Port5 & 0x08) != 0;
}
bool GameGearIOPorts::isReceiveEnabled() const
{
    // RON (Bit 5) for Receive Enable
    return (m_Port5 & 0x20) != 0;
}

bool GameGearIOPorts::isTransmitEnabled() const
{
    return (m_Port5 & 0x10) != 0;  // TON (Bit 4)
}



u8 GameGearIOPorts::scrambleParallelBits(u8 data) const
{
    // Bit-Scrambling gemäß SMSPower Dokument
    u8 result = 0;

    // Scrambling-Tabelle:
    // Sent Bit -> Received Bit
    // 0 -> 2, 1 -> 3, 2 -> 1, 3 -> 0
    // 4 -> 5, 5 -> 4, 6 -> 6, 7 -> NC

    if (data & 0x01) result |= 0x04;  // Bit 0 -> Bit 2
    if (data & 0x02) result |= 0x08;  // Bit 1 -> Bit 3
    if (data & 0x04) result |= 0x02;  // Bit 2 -> Bit 1
    if (data & 0x08) result |= 0x01;  // Bit 3 -> Bit 0
    if (data & 0x10) result |= 0x20;  // Bit 4 -> Bit 5
    if (data & 0x20) result |= 0x10;  // Bit 5 -> Bit 4
    if (data & 0x40) result |= 0x40;  // Bit 6 -> Bit 6
    // Bit 7 nicht verbunden

    return result;
}

u8 GameGearIOPorts::unscrambleParallelBits(u8 data) const
{
    // Inverse Scrambling-Tabelle
    u8 result = 0;

    if (data & 0x04) result |= 0x01;  // Bit 2 -> Bit 0
    if (data & 0x08) result |= 0x02;  // Bit 3 -> Bit 1
    if (data & 0x02) result |= 0x04;  // Bit 1 -> Bit 2
    if (data & 0x01) result |= 0x08;  // Bit 0 -> Bit 3
    if (data & 0x20) result |= 0x10;  // Bit 5 -> Bit 4
    if (data & 0x10) result |= 0x20;  // Bit 4 -> Bit 5
    if (data & 0x40) result |= 0x40;  // Bit 6 -> Bit 6

    return result;
}



bool GameGearIOPorts::hasTriggeredParallelNMI(u8 oldValue, u8 newValue){
    if((m_Port2 & 0x80) == 0x00) // NINT enabled
    {
        return  ((oldValue & 0x40) != 0) &&     // Bit 6 was Input
                    ((newValue & 0x40) == 0);    // Bit 6 becomes Output
    }
return false;
}

bool GameGearIOPorts::isOutputBit(u8 bit) {
    if (bit == 4 && getBit(  m_Port5, 4)) {
        return true;
    }
    if (bit == 5 && getBit(m_Port5, 5)) {
        return false;
    }
    if (getBit(m_Port2, bit)) return false;
    return true;
}