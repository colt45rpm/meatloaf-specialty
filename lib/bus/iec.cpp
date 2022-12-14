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

#include "iec.h"

#include "../../include/debug.h"
#include "../../include/pinmap.h"
#include "../../include/cbmdefines.h"

#include "drive.h"
#include "device_db.h"

#include "string_utils.h"

iecBus IEC;

using namespace CBM;
using namespace Protocol;


static void IRAM_ATTR cbm_on_attention_isr_handler(void* arg)
{
    iecBus *b = (iecBus *)arg;

    // Go to listener mode and get command
    b->protocol->release ( PIN_IEC_CLK_OUT );
    b->protocol->pull ( PIN_IEC_DATA_OUT );

    b->protocol->flags or_eq ATN_PULLED;
    b->bus_state = BUS_ACTIVE;
}

static void ml_iec_intr_task(void* arg)
{
    while ( true ) 
    {
        if ( IEC.enabled )
        {
            IEC.service();
            if ( IEC.bus_state < BUS_ACTIVE )
                taskYIELD();
        }
    }
}


void iecBus::setup()
{
    // Start task
    //xTaskCreate(ml_iec_intr_task, "ml_iec_intr_task", 2048, NULL, 10, NULL);
    xTaskCreatePinnedToCore(ml_iec_intr_task, "ml_iec_intr_task", 4096, NULL, 10, NULL, 1);

    // Setup interrupt for ATN
    gpio_config_t io_conf = {
        .pin_bit_mask = ( 1ULL << PIN_IEC_ATN ),    // bit mask of the pins that you want to set
        .mode = GPIO_MODE_INPUT,                    // set as input mode
        .pull_up_en = GPIO_PULLUP_DISABLE,          // disable pull-up mode
        .pull_down_en = GPIO_PULLDOWN_DISABLE,      // disable pull-down mode
        .intr_type = GPIO_INTR_NEGEDGE              // interrupt of falling edge
    };
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    gpio_isr_handler_add((gpio_num_t)PIN_IEC_ATN, cbm_on_attention_isr_handler, this);
}


/********************************************************
 *
 *  IEC Device Functions
 *
 ********************************************************/

iecDevice::iecDevice ( void ) { } // ctor


device_state_t iecDevice::queue_command ( void )
{
    // Record command for this device
    this->data = IEC.data;
    // this->data.primary = IEC.data.primary;
    // this->data.secondary = IEC.data.secondary;
    // this->data.device = IEC.data.device;
    // this->data.channel = IEC.data.channel;
    // this->data.primary = IEC.data.primary;
    // this->data.device_command = IEC.data.device_command;

    if ( this->data.primary == IEC_LISTEN )
    {
        device_state = DEVICE_LISTEN;
    }
    else if ( this->data.primary == IEC_TALK )
    {
        device_state = DEVICE_TALK;
    }

    // Debug_printf("DEV primary[%.2X] secondary[%.2X] device[%d], channel[%d] command[%s] ", this->data.primary, this->data.secondary, this->data.device, this->data.channel, this->data.device_command.c_str());

    return device_state;
}


std::shared_ptr<MStream> iecDevice::retrieveStream ( void )
{
    size_t key = ( this->data.device * 100 ) + this->data.channel;
    Debug_printv("Stream key[%d]", key);

    if ( streams.find ( key ) != streams.end() )
    {
        //Debug_printv("Stream retrieved. key[%d]", key);
        return streams.at ( key );
    }
    else
    {
		//Debug_printv("Error! Trying to recall not-registered stream!");
        return nullptr;
    }
}

// used to start working with a stream, registering it as underlying stream of some
// IEC channel on some IEC device
bool iecDevice::registerStream (std::ios_base::open_mode mode)
{
    // Debug_printv("dc_basepath[%s]",  device_config.basepath().c_str());
    // Debug_printv("m_filename[%s]", m_filename.c_str());
    // //auto file = Meat::New<MFile>( device_config.basepath() + "/" + m_filename );
    // auto file = Meat::New<MFile>( m_mfile->url + m_filename );
    auto file = Meat::New<MFile>( m_filename );
    if ( !file->exists() )
        return false;

    std::shared_ptr<MStream> new_stream;

    // LOAD / GET / INPUT
    if ( mode == std::ios_base::in )
    {
        Debug_printv("LOAD m_mfile[%s] m_filename[%s]", m_mfile->url.c_str(), m_filename.c_str());
        new_stream = std::shared_ptr<MStream>(file->meatStream());

        if ( new_stream == nullptr )
        {
            return false;
        }

        if( !new_stream->isOpen() )
        {
            //Debug_printv("Error creating istream");
            return false;
        }
        else
        {
            // Close the stream if it is already open
            closeStream();
        }
    }

    // SAVE / PUT / PRINT / WRITE
    else
    {
        Debug_printv("SAVE m_filename[%s]", m_filename.c_str());
        // CREATE STREAM HERE FOR OUTPUT
        return false;
    }


    size_t key = ( this->data.device * 100 ) + this->data.channel;
    auto newPair = std::make_pair ( key, new_stream );
    streams.insert ( newPair );

    //Debug_printv("Stream created. key[%d]", key);
    return true;
}

bool iecDevice::closeStream ( bool close_all )
{
    size_t key = ( this->data.device * 100 ) + this->data.channel;
    auto found = streams.find(key);

    if ( found != streams.end() )
    {
        //Debug_printv("Stream closed. key[%d]", key);
        auto closingStream = (*found).second;
        closingStream->close();
        return streams.erase ( key );
    }

    return false;
}


/********************************************************
 *
 *  IEC Bus Functions
 *
 ********************************************************/


iecBus::iecBus ( void )
{
    init();
} // ctor

// Set all IEC_signal lines in the correct mode
//
bool iecBus::init()
{

#ifndef IEC_SPLIT_LINES
    // make sure the output states are initially LOW
//  protocol->release(PIN_IEC_ATN);
    protocol->release ( PIN_IEC_CLK_OUT );
    protocol->release ( PIN_IEC_DATA_OUT );
    protocol->release ( PIN_IEC_SRQ );

    // initial pin modes in GPIO
    protocol->set_pin_mode ( PIN_IEC_ATN, INPUT );
    protocol->set_pin_mode ( PIN_IEC_CLK_IN, INPUT );
    protocol->set_pin_mode ( PIN_IEC_DATA_IN, INPUT );
    protocol->set_pin_mode ( PIN_IEC_SRQ, INPUT );
    protocol->set_pin_mode ( PIN_IEC_RESET, INPUT );
#else
    // make sure the output states are initially LOW
    // protocol->release(PIN_IEC_ATN);
    // protocol->release(PIN_IEC_CLK_IN);
    // protocol->release(PIN_IEC_CLK_OUT);
    // protocol->release(PIN_IEC_DATA_IN);
    // protocol->release(PIN_IEC_DATA_OUT);
    // protocol->release(PIN_IEC_SRQ);

    // initial pin modes in GPIO
    protocol->set_pin_mode ( PIN_IEC_ATN, INPUT );
    protocol->set_pin_mode ( PIN_IEC_CLK_IN, INPUT );
    protocol->set_pin_mode ( PIN_IEC_CLK_OUT, OUTPUT );
    protocol->set_pin_mode ( PIN_IEC_DATA_IN, INPUT );
    protocol->set_pin_mode ( PIN_IEC_DATA_OUT, OUTPUT );
    protocol->set_pin_mode ( PIN_IEC_SRQ, OUTPUT );
    protocol->set_pin_mode ( PIN_IEC_RESET, INPUT );
#endif

    active_protocol = PROTOCOL_CBM_SERIAL;
    selectProtocol();
    protocol->flags = CLEAR;

    return true;
} // init



/******************************************************************************
 *                                                                             *
 *                               Public functions                              *
 *                                                                             *
 ******************************************************************************/

// This function checks and deals with atn signal commands
//
// If a command is recieved, the this->data.string is saved in this->data. Only commands
// for *this* device are dealt with.
//
/** from Derogee's "IEC Disected"
 * ATN SEQUENCES
 * When ATN is PULLED true, everybody stops what they are doing. The processor will quickly protocol->pull the
 * Clock line true (it's going to send soon), so it may be hard to notice that all other devices protocol->release the
 * Clock line. At the same time, the processor protocol->releases the Data line to false, but all other devices are
 * getting ready to listen and will each protocol->pull Data to true. They had better do this within one
 * millisecond (1000 microseconds), since the processor is watching and may sound an alarm ("device
 * not available") if it doesn't see this take place. Under normal circumstances, transmission now
 * takes place as previously described. The computer is sending commands rather than data, but the
 * characters are exchanged with exactly the same timing and handshakes as before. All devices
 * receive the commands, but only the specified device acts upon it. This results in a curious
 * situation: you can send a command to a nonexistent device (try "OPEN 6,6") - and the computer
 * will not know that there is a problem, since it receives valid handshakes from the other devices.
 * The computer will notice a problem when you try to send or receive data from the nonexistent
 * device, since the unselected devices will have dropped off when ATN ceased, leaving you with
 * nobody to talk to.
 */
// Return value, see bus_state_t definition.

void iecBus::service ( void )
{
    bool process_command = false;
    bool pin_atn = protocol->status ( PIN_IEC_ATN );

#ifdef IEC_HAS_RESET

    // Check if CBM is sending a reset (setting the RESET line high). This is typically
    // when the CBM is reset itself. In this case, we are supposed to reset all states to initial.
    bool pin_reset = protocol->status ( PIN_IEC_RESET );
    if ( pin_reset == PULLED )
    {
        if ( pin_atn == PULLED )
        {
            // If RESET & ATN are both PULLED then CBM is off
            this->bus_state = BUS_OFFLINE;
            return;
        }

        Debug_printf ( "IEC Reset! reset[%d] atn[%d]\r\n", pin_reset, pin_atn );
        this->data.init(); // Clear bus data
        this->bus_state = BUS_IDLE;

        // Reset virtual devices
        drive.reset();

        return;
    }

#endif

    if ( this->bus_state == BUS_OFFLINE && pin_atn )
        pin_atn = false;

    // Command or Data Mode
    if ( this->bus_state == BUS_ACTIVE || pin_atn )
    {
        protocol->release ( PIN_IEC_CLK_OUT );
        protocol->pull ( PIN_IEC_DATA_OUT );

        // ATN was pulled read control code from the bus
        // protocol->pull ( PIN_IEC_SRQ );
        int16_t c = ( bus_command_t ) receive();
        // protocol->release ( PIN_IEC_SRQ );

        // Check for error
        if ( c == 0xFFFFFFFF || protocol->flags bitand ERROR )
        {
            //Debug_printv ( "Error reading command" );            
            if ( c == 0xFFFFFFFF )
                this->bus_state = BUS_OFFLINE;
            else

                this->bus_state = BUS_ERROR;
        }
        else
        {
            // Debug_println ( "COMMAND MODE" );        

            Debug_printf ( "   IEC: [%.2X]", c );

            // Check for JiffyDOS
            if ( protocol->flags bitand JIFFY_ACTIVE )
            {
                Debug_printf ( "[JIFFY]" );
                active_protocol = PROTOCOL_JIFFYDOS;
            }
            if ( protocol->flags bitand DOLPHIN_ACTIVE )
            {
                Debug_printf ( "[DOLPHIN]" );
                active_protocol = PROTOCOL_DOLPHINDOS;
            }

            // Decode command byte
            uint8_t command = c bitand 0xF0;
            if ( c == IEC_UNLISTEN ) command = IEC_UNLISTEN;
            if ( c == IEC_UNTALK ) command = IEC_UNTALK;

            switch ( command ) {
                case IEC_GLOBAL:
                    this->data.primary = IEC_GLOBAL;
                    this->data.device = c xor IEC_GLOBAL;
                    this->bus_state = BUS_IDLE;
                    Debug_printf ( " (00 GLOBAL %.2d COMMAND)\r\n", this->data.device );
                    break;

                case IEC_LISTEN:
                    this->data.primary = IEC_LISTEN;
                    this->data.device = c xor IEC_LISTEN;
                    this->data.secondary = IEC_REOPEN;    // Default secondary command
                    this->data.channel = CMD_CHANNEL;   // Default channel
                    this->bus_state = BUS_ACTIVE;
                    Debug_printf ( " (20 LISTEN %.2d DEVICE)\r\n", this->data.device );
                    break;

                case IEC_UNLISTEN:
                    this->data.primary = IEC_UNLISTEN;
                    this->data.secondary = 0x00;
                    this->bus_state = BUS_IDLE;
                    Debug_printf ( " (3F UNLISTEN)\r\n" );
                    break;

                case IEC_TALK:
                    this->data.primary = IEC_TALK;
                    this->data.device = c xor IEC_TALK;
                    this->data.secondary = IEC_REOPEN;    // Default secondary command
                    this->data.channel = CMD_CHANNEL;   // Default channel
                    this->bus_state = BUS_ACTIVE;
                    Debug_printf ( " (40 TALK   %.2d DEVICE)\r\n", this->data.device );
                    break;

                case IEC_UNTALK:
                    this->data.primary = IEC_UNTALK;
                    this->data.secondary = 0x00;
                    this->bus_state = BUS_IDLE;
                    Debug_printf ( " (5F UNTALK)\r\n" );
                    break;

                case IEC_OPEN:
                    this->data.secondary = IEC_OPEN;
                    this->data.channel = c xor IEC_OPEN;
                    this->bus_state = BUS_PROCESS;
                    process_command = true;
                    Debug_printf ( " (F0 OPEN   %.2d CHANNEL)\r\n", this->data.channel );
                    break;

                case IEC_REOPEN:
                    this->data.secondary = IEC_REOPEN;
                    this->data.channel = c xor IEC_REOPEN;
                    this->bus_state = BUS_PROCESS;
                    process_command = true;
                    Debug_printf ( " (60 DATA   %.2d CHANNEL)\r\n", this->data.channel );
                    break;

                case IEC_CLOSE:
                    this->data.secondary = IEC_CLOSE;
                    this->data.channel = c xor IEC_CLOSE;
                    this->bus_state = BUS_PROCESS;
                    process_command = true;
                    Debug_printf ( " (E0 CLOSE  %.2d CHANNEL)\r\n", this->data.channel );
                    break; 
            }

            // Debug_printf ( "code[%.2X] primary[%.2X] secondary[%.2X] bus[%d]", command, this->data.primary, this->data.secondary, this->bus_state );

            // Is this command for us?
            if ( !isDeviceEnabled( this->data.device ) )
            {
                this->bus_state = BUS_IDLE;
                process_command = false;
            }

        }

        // If the bus is idle then release the lines
        if ( this->bus_state < BUS_ACTIVE )
        {
            //Debug_printv("release lines");
            this->data.init();
            releaseLines();
        }

        // Debug_printf ( "code[%.2X] primary[%.2X] secondary[%.2X] bus[%d]", command, this->data.primary, this->data.secondary, this->bus_state );
        // Debug_printf( "primary[%.2X] secondary[%.2X] bus_state[%d]", this->data.primary, this->data.secondary, this->bus_state );
        // protocol->release ( PIN_IEC_SRQ );
    }
    else if ( this->bus_state > BUS_IDLE )
    {
        // If no secondary was set, process primary with defaults
        if ( this->data.primary > IEC_GLOBAL )
        {
            process_command = true;
        }
    }

    if ( process_command )
    {
        // protocol->pull( PIN_IEC_SRQ );
        // Debug_println ( "DATA MODE" );
        // Debug_printf ( "bus[%d] device[%d] primary[%d] secondary[%d]", this->bus_state, this->device_state, this->data.primary, this->data.secondary );

        if ( this->data.secondary == IEC_OPEN || this->data.secondary == IEC_REOPEN )
        {
#ifdef PARALLEL_BUS
            //PARALLEL.handShake();
#endif

            // Switch to detected protocol
            // active_protocol = PROTOCOL_DOLPHINDOS;
            selectProtocol();
        }

        // Data Mode - Get Command or Data
        if ( this->data.primary == IEC_LISTEN )
        {
            //Debug_printv( "deviceListen" );
            PARALLEL.setMode( MODE_RECEIVE );
            this->bus_state = deviceListen();
        }
        else if ( this->data.primary == IEC_TALK )
        {
            //Debug_printv( "deviceTalk" );
            //Debug_printf ( " (40 TALK   %.2d DEVICE %.2x SECONDARY %.2d CHANNEL)\r\n", this->data.device, this->data.secondary, this->data.channel );
            PARALLEL.setMode( MODE_SEND );
            this->bus_state = deviceTalk();   
        }

        // Queue control codes and command in specified device
        // At the moment there is only the multi-drive device
        device_state_t device_state = drive.queue_command();

        // Process commands in devices
        // At the moment there is only the multi-drive device
        //Debug_printv( "deviceProcess" );
        if ( drive.process() < DEVICE_ACTIVE || device_state < DEVICE_ACTIVE )
        {
            //Debug_printv("device idle");
            this->data.init();
        }

        this->bus_state = BUS_IDLE;

        // Switch back to standard serial
        active_protocol = PROTOCOL_CBM_SERIAL;
        selectProtocol();
        protocol->flags = CLEAR;

        // Set Parallel back to receive
        //PARALLEL.setMode( MODE_RECEIVE );

        // Debug_printf( "primary[%.2X] secondary[%.2X] bus_state[%d]", this->data.primary, this->data.secondary, this->bus_state );
        // Debug_printf( "atn[%d] clk[%d] data[%d] srq[%d]", IEC.protocol->status(PIN_IEC_ATN), IEC.protocol->status(PIN_IEC_CLK_IN), IEC.protocol->status(PIN_IEC_DATA_IN), IEC.protocol->status(PIN_IEC_SRQ));
        // protocol->release ( PIN_IEC_SRQ );
    }


    //Debug_printf("command[%.2X] device[%.2d] secondary[%.2d] channel[%.2d]", this->data.primary, this->data.device, this->data.secondary, this->data.channel);
} // service

bus_state_t iecBus::deviceListen ( void )
{
    // Okay, we will listen.
    // Debug_printf( "secondary[%d] channel[%d]", this->data.secondary, this->data.channel );

    // If the command is SECONDARY and it is not to expect just a small command on the command channel, then
    // we're into something more heavy. Otherwise read it all out right here until UNLISTEN is received.
    if ( this->data.secondary == IEC_REOPEN && this->data.channel != CMD_CHANNEL )
    {
        // A heapload of data might come now, too big for this context to handle so the caller handles this, we're done here.
        // Debug_printf(" (%.2X SECONDARY) (%.2X CHANNEL)\r\n", this->data.primary, this->data.channel);
        Debug_printf ( "\r\n" );
        return BUS_ACTIVE;
    }

    // OPEN or DATA
    else if ( this->data.secondary == IEC_OPEN || this->data.secondary == IEC_REOPEN )
    {
        // Record the command string until ATN is PULLED
        std::string listen_command = "";

        // ATN might get pulled right away if there is no command string to send
        protocol->wait( TIMING_STABLE );

        while ( protocol->status ( PIN_IEC_ATN ) != PULLED )
        {
            int16_t c = receive();

            if ( protocol->flags bitand EMPTY_STREAM )
                break;

            if ( protocol->flags bitand ERROR )
            {
                Debug_printf ( "Some other command [%.2X]", c );
                return BUS_ERROR;
            }

            if ( c != 0x0D && c != 0xFFFFFFFF )
            {
                listen_command += ( uint8_t ) c;
            }

            if ( protocol->flags bitand EOI_RECVD )
                break;
        }

        if ( listen_command.length() )
        {
            // remove media id from command
            if ( listen_command[0]=='0' && listen_command[1]==':' )
                listen_command = mstr::drop(listen_command, 2);

            this->data.device_command = listen_command;
            mstr::rtrimA0 ( this->data.device_command );
            Debug_printf ( " {%s}\r\n", this->data.device_command.c_str() );
            //return BUS_PROCESS;
        }
        else
        {
            this->data.device_command = "";
            Debug_printf ( "\r\n" );            
        }

        //Debug_printv("listen complete");
        return BUS_PROCESS;
    }

    // CLOSE Named Channel
    else if ( this->data.secondary == IEC_CLOSE )
    {
        // Debug_printf(" (E0 CLOSE) (%d CHANNEL)\r\n", this->data.channel);
        return BUS_PROCESS;
    }

    // Unknown
    else
    {
        Debug_printf ( " OTHER (%.2X COMMAND) (%.2X CHANNEL) ", this->data.secondary, this->data.channel );
        return BUS_ERROR;
    }
}


bus_state_t iecBus::deviceTalk ( void )
{
    // Okay, we will talk soon
    // Debug_printf(" (%.2X SECONDARY) (%.2X CHANNEL)\r\n", this->data.primary, this->data.channel);

    // Now do bus turnaround
    if ( not turnAround() )
        return BUS_ERROR;

    // We have recieved a CMD and we should talk now:
    return BUS_PROCESS;
}


// IEC turnaround
bool iecBus::turnAround ( void )
{
    /*
    TURNAROUND
    An unusual sequence takes place following ATN if the computer wishes the remote device to
    become a talker. This will usually take place only after a Talk command has been sent.
    Immediately after ATN is RELEASED, the selected device will be behaving like a listener. After all, it's
    been listening during the ATN cycle, and the computer has been a talker. At this instant, we
    have "wrong way" logic; the device is holding down the Data line, and the computer is holding the
    Clock line. We must turn this around. Here's the sequence:
    the computer quickly realizes what's going on, and pulls the Data line to true (it's already there), as
    well as releasing the Clock line to false. The device waits for this: when it sees the Clock line go
    true [sic], it releases the Data line (which stays true anyway since the computer is now holding it down)
    and then pulls down the Clock line. We're now in our starting position, with the talker (that's the
    device) holding the Clock true, and the listener (the computer) holding the Data line true. The
    computer watches for this state; only when it has gone through the cycle correctly will it be ready
    to receive data. And data will be signalled, of course, with the usual sequence: the talker releases
    the Clock line to signal that it's ready to send.
    */
    // Debug_printf("IEC turnAround: ");

    // Wait until the computer releases the ATN line
    if ( protocol->timeoutWait ( PIN_IEC_ATN, RELEASED, FOREVER ) == TIMED_OUT )
    {
        Debug_printf("Wait until the computer releases the ATN line");
        protocol->flags or_eq ERROR;
        return -1; // return error because timeout
    }

    // Delay after ATN is RELEASED
    delayMicroseconds(TIMING_Ttk);

    // Wait until the computer releases the clock line
    if ( protocol->timeoutWait ( PIN_IEC_CLK_IN, RELEASED, FOREVER ) == TIMED_OUT )
    {
        Debug_printf ( "Wait until the computer releases the clock line" );
        protocol->flags or_eq ERROR;
        return -1; // return error because timeout
    }

    protocol->release ( PIN_IEC_DATA_OUT );
    delayMicroseconds ( TIMING_Tv );
    protocol->pull ( PIN_IEC_CLK_OUT );
    delayMicroseconds ( TIMING_Tv );

    //Debug_println("turnaround complete");
    return true;
} // turnAround


// this routine will set the direction on the bus back to normal
// (the way it was when the computer was switched on)
bool iecBus::undoTurnAround ( void )
{
    protocol->pull ( PIN_IEC_DATA_OUT );
    protocol->release ( PIN_IEC_CLK_OUT );

    // Debug_printf("IEC undoTurnAround: ");

    // Wait until the computer releases the clock line
    if ( protocol->timeoutWait ( PIN_IEC_CLK_IN, RELEASED, FOREVER ) == TIMED_OUT )
    {
        Debug_printf ( "Wait until the computer releases the clock line" );
        protocol->flags or_eq ERROR;
        return -1; // return error because timeout
    }

    // Debug_println("complete");
    return true;
} // undoTurnAround


void iecBus::releaseLines ( bool wait )
{
    // IEC.protocol->pull ( PIN_IEC_SRQ );

    // Wait for ATN to release and quit
    if ( wait )
    {
        //Debug_printf("Waiting for ATN to release");
        IEC.protocol->timeoutWait ( PIN_IEC_ATN, RELEASED, FOREVER );
    }

    // Release lines
    protocol->release ( PIN_IEC_CLK_OUT );
    protocol->release ( PIN_IEC_DATA_OUT );

// #ifndef SPLIT_LINES
//     protocol->set_pin_mode ( PIN_IEC_CLK_OUT, INPUT );
//     protocol->set_pin_mode ( PIN_IEC_DATA_OUT, INPUT );
// #endif

    // IEC.protocol->release ( PIN_IEC_SRQ );
}


// boolean  iecBus::checkRESET()
// {
//  return readRESET();
//  return false;
// } // checkRESET


// IEC_receive receives a byte
//
int16_t iecBus::receive ()
{
    int16_t data;

    data = protocol->receiveByte(); // Standard CBM Timing

#ifdef DATA_STREAM
    if ( !(protocol->flags bitand ERROR) )
        Debug_printf ( "%.2X ", data );
#endif

    return data;
} // receive


// IEC_send sends a byte
//
bool iecBus::send ( uint8_t data )
{
#ifdef DATA_STREAM
    Debug_printf ( "%.2X ", data );
#endif
    bool r = protocol->sendByte ( data, false ); // Standard CBM Timing

    return r;
} // send

size_t iecBus::send ( std::string data, size_t offset )
{
    size_t i = 0;
    for ( i = offset; i < data.length(); ++i )
        if ( !send ( ( uint8_t ) data[i] ) )
            return i;

    IEC.sendEOI('\x0D');

    return i;
}


// Same as IEC_send, but indicating that this is the last byte.
//
bool iecBus::sendEOI ( uint8_t data )
{
#ifdef DATA_STREAM
    Debug_printf ( "%.2X ", data );
#endif
    //Debug_println ( "\r\nEOI Sent!" );

    //IEC.protocol->pull(PIN_IEC_SRQ);
    bool r = protocol->sendByte ( data, true ); // Standard CBM Timing
    //IEC.protocol->release(PIN_IEC_SRQ);

    releaseLines();
    //Debug_printv("release lines");
    this->bus_state = BUS_IDLE;

    // BETWEEN BYTES TIME
    delayMicroseconds ( TIMING_Tbb );

    return r;
} // sendEOI


// Informs listener(s) there is no data to receive
bool iecBus::senderTimeout()
{
    //protocol->pull( PIN_IEC_SRQ );
    // Message file not found by just releasing lines
    releaseLines();
    //Debug_printv("release lines");
    this->bus_state = BUS_ERROR;

    // Signal an empty stream
    delayMicroseconds ( TIMING_EMPTY );

    //protocol->release( PIN_IEC_SRQ );
    return true;
} // senderTimeout


bool iecBus::isDeviceEnabled ( const uint8_t deviceNumber )
{
    return ( enabledDevices & ( 1 << deviceNumber ) );
} // isDeviceEnabled

void iecBus::enableDevice ( const uint8_t deviceNumber )
{
    enabledDevices |= 1UL << deviceNumber;
    protocol->enabledDevices = enabledDevices;
} // enableDevice

void iecBus::disableDevice ( const uint8_t deviceNumber )
{
    enabledDevices &= ~ ( 1UL << deviceNumber );
    protocol->enabledDevices = enabledDevices;
} // disableDevice






void iecBus::shutdown()
{
    // for (auto devicep : _daisyChain)
    // {
    //     Debug_printf("Shutting down device %02x\n", devicep.second->id());
    //     devicep.second->shutdown();
    // }
    // Debug_printf("All devices shut down.\n");
}

// Add device to SIO bus
void iecBus::addDevice ( iecDevice *pDevice, uint8_t device_id )
{
    // if (device_id == SIO_DEVICEID_FUJINET)
    // {
    //     _fujiDev = (sioFuji *)pDevice;
    // }
    // else if (device_id == SIO_DEVICEID_RS232)
    // {
    //     _modemDev = (sioModem *)pDevice;
    // }
    // else if (device_id >= SIO_DEVICEID_FN_NETWORK && device_id <= SIO_DEVICEID_FN_NETWORK_LAST)
    // {
    //     _netDev[device_id - SIO_DEVICEID_FN_NETWORK] = (sioNetwork *)pDevice;
    // }
    // else if (device_id == SIO_DEVICEID_MIDI)
    // {
    //     _midiDev = (sioMIDIMaze *)pDevice;
    // }
    // else if (device_id == SIO_DEVICEID_CASSETTE)
    // {
    //     _cassetteDev = (sioCassette *)pDevice;
    // }
    // else if (device_id == SIO_DEVICEID_CPM)
    // {
    //     _cpmDev = (sioCPM *)pDevice;
    // }
    // else if (device_id == SIO_DEVICEID_PRINTER)
    // {
    //     _printerdev = (sioPrinter *)pDevice;
    // }

    pDevice->device_id = device_id;

    _daisyChain.push_front ( pDevice );
}

// Removes device from the SIO bus.
// Note that the destructor is called on the device!
void iecBus::remDevice ( iecDevice *p )
{
    _daisyChain.remove ( p );
}

// Should avoid using this as it requires counting through the list
uint8_t iecBus::numDevices()
{
    int i = 0;

    for ( auto devicep : _daisyChain )
        i++;

    return i;
}

void iecBus::changeDeviceId ( iecDevice *p, uint8_t device_id )
{
    for ( auto devicep : _daisyChain )
    {
        if ( devicep == p )
            devicep->device_id = device_id;
    }
}

iecDevice *iecBus::deviceById ( uint8_t device_id )
{
    for ( auto devicep : _daisyChain )
    {
        if ( devicep->device_id == device_id )
            return devicep;
    }

    return nullptr;
}



void iecBus::debugTiming()
{
    uint8_t pin = PIN_IEC_ATN;

#ifndef SPILIT_LINES
    protocol->pull ( pin );
    delayMicroseconds ( 1000 ); // 1000
    protocol->release ( pin );
    delayMicroseconds ( 1000 );
#endif

    pin = PIN_IEC_CLK_OUT;
    protocol->pull ( pin );
    delayMicroseconds ( 20 ); // 20
    protocol->release ( pin );
    delayMicroseconds ( 20 );

    pin = PIN_IEC_DATA_OUT;
    protocol->pull ( pin );
    delayMicroseconds ( 40 ); // 40
    protocol->release ( pin );
    delayMicroseconds ( 40 );

    pin = PIN_IEC_SRQ;
    protocol->pull ( pin );
    delayMicroseconds ( 60 ); // 60
    protocol->release ( pin );
    delayMicroseconds ( 60 );
}
