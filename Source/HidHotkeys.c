/******************************************************************************

 @file  HidHotkeys.c

 @brief This file contains the HID emulated keyboard sample application for use
        with the CC2540 Bluetooth Low Energy Protocol Stack.

 Group: WCS, BTS
 Target Device: CC2540, CC2541

 ******************************************************************************
 
 Copyright (c) 2011-2021, Texas Instruments Incorporated
 All rights reserved.

 IMPORTANT: Your use of this Software is limited to those specific rights
 granted under the terms of a software license agreement between the user
 who downloaded the software, his/her employer (which must be your employer)
 and Texas Instruments Incorporated (the "License"). You may not use this
 Software unless you agree to abide by the terms of the License. The License
 limits your use, and you acknowledge, that the Software may not be modified,
 copied or distributed unless embedded on a Texas Instruments microcontroller
 or used solely and exclusively in conjunction with a Texas Instruments radio
 frequency transceiver, which is integrated into your product. Other than for
 the foregoing purpose, you may not use, reproduce, copy, prepare derivative
 works of, modify, distribute, perform, display or sell this Software and/or
 its documentation for any purpose.

 YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
 PROVIDED �AS IS� WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
 NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
 TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
 NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
 LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
 INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
 OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
 OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
 (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

 Should you have any questions regarding your right to use this Software,
 contact Texas Instruments Incorporated at www.TI.com.

 ******************************************************************************
 Release Name: ble_sdk_1.5.2.0
 Release Date: 2021-03-10 17:38:59
 *****************************************************************************/


/*********************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "OnBoard.h"

#include "hal_drivers.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_adc.h"

#include "gatt.h"
#include "hci.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "gatt_profile_uuid.h"
#include "linkdb.h"
#include "peripheral.h"
#include "gapbondmgr.h"
#include "hidkbdservice.h"
#include "devinfoservice.h"
#include "battservice.h"
#include "hiddev.h"
#include "HidHotkeys.h"
#ifdef SCAN_PARAM_NOTIFY_TEST
  #include "scanparamservice.h"
#endif // SCAN_PARAM_NOTIFY_TEST

/*********************************************************************
 * MACROS
 */

// Selected HID keycodes
#define KEY_RIGHT_ARROW             0x4F
#define KEY_LEFT_ARROW              0x50
#define KEY_NONE                    0x00

// Selected HID LED bitmaps
#define LED_NUM_LOCK                0x01
#define LED_CAPS_LOCK               0x02

// Selected HID mouse button values
#define MOUSE_BUTTON_1              0x01
#define MOUSE_BUTTON_NONE           0x00

// HID keyboard input report length
#define HID_KEYBOARD_IN_RPT_LEN     8

// HID LED output report length
#define HID_LED_OUT_RPT_LEN         1

// HID mouse input report length
#define HID_MOUSE_IN_RPT_LEN        5

/*********************************************************************
 * CONSTANTS
 */

// HID idle timeout in msec; set to zero to disable timeout
#define DEFAULT_HID_IDLE_TIMEOUT              60000

// Minimum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL     8

// Maximum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL     8

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_DESIRED_SLAVE_LATENCY         50

// Supervision timeout value (units of 10ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_CONN_TIMEOUT          500

// Whether to enable automatic parameter update request when a connection is formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         TRUE

// Connection Pause Peripheral time value (in seconds)
#define DEFAULT_CONN_PAUSE_PERIPHERAL         10

// Default passcode
#define DEFAULT_PASSCODE                      0

// Default GAP pairing mode
#define DEFAULT_PAIRING_MODE                  GAPBOND_PAIRING_MODE_INITIATE

// Default MITM mode (TRUE to require passcode or OOB when pairing)
#define DEFAULT_MITM_MODE                     FALSE

// Default bonding mode, TRUE to bond
#define DEFAULT_BONDING_MODE                  TRUE

// Default GAP bonding I/O capabilities
#define DEFAULT_IO_CAPABILITIES               GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT

// Battery level is critical when it is less than this %
#define DEFAULT_BATT_CRITICAL_LEVEL           6

// ADC voltage levels
#define BATT_ADC_LEVEL_3_0V                   409 // 3.0 V
#define BATT_ADC_LEVEL_2_0V                   273 // 2.0 V
/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

// Task ID
uint8 HidHotkeysTaskId;

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// GAP Profile - Name attribute for SCAN RSP data
static uint8 scanData[] =
{
  0x0D,                             // length of this data
  GAP_ADTYPE_LOCAL_NAME_COMPLETE,   // AD Type = Complete local name
  'H',
  'I',
  'D',
  ' ',
  'K',
  'e',
  'y',
  'b',
  'o',
  'a',
  'r',
  'd'
};

// Advertising data
static uint8 advData[] =
{
  // flags
  0x02,   // length of this data
  GAP_ADTYPE_FLAGS,
  GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

  // appearance
  0x03,   // length of this data
  GAP_ADTYPE_APPEARANCE,
  LO_UINT16(GAP_APPEARE_HID_KEYBOARD),
  HI_UINT16(GAP_APPEARE_HID_KEYBOARD),

  // service UUIDs
  0x05,   // length of this data
  GAP_ADTYPE_16BIT_MORE,
  LO_UINT16(HID_SERV_UUID),
  HI_UINT16(HID_SERV_UUID),
  LO_UINT16(BATT_SERV_UUID),
  HI_UINT16(BATT_SERV_UUID)
};

// Device name attribute value
static CONST uint8 attDeviceName[GAP_DEVICE_NAME_LEN] = "HID Keyboard";

// HID Dev configuration
static hidDevCfg_t HidHotkeysCfg =
{
  DEFAULT_HID_IDLE_TIMEOUT,   // Idle timeout
  HID_KBD_FLAGS               // HID feature flags
};

// TRUE if boot mouse enabled
static uint8 hidBootMouseEnabled = FALSE;

/*********************************************************************
 * LOCAL FUNCTIONS
 */

static void HidHotkeys_ProcessOSALMsg( osal_event_hdr_t *pMsg );
static void HidHotkeys_HandleKeys( uint8 shift, uint8 keys );
static void HidHotkeys_ProcessGattMsg( gattMsgEvent_t *pMsg );
static void HidHotkeysSendReport( uint8 keycode );
static void HidHotkeysSendMouseReport( uint8 buttons );
static uint8 HidHotkeysRcvReport( uint8 len, uint8 *pData );
static uint8 HidHotkeysRptCB( uint8 id, uint8 type, uint16 uuid,
                             uint8 oper, uint8 *pLen, uint8 *pData );
static void HidHotkeysEvtCB( uint8 evt );
static void HidHotkeysBattMeasSetupCB( void );
static void HidHotkeysBattMeasTeardownCB( void );

/*********************************************************************
 * PROFILE CALLBACKS
 */

static hidDevCB_t HidHotkeysHidCBs =
{
  HidHotkeysRptCB,
  HidHotkeysEvtCB,
  NULL
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      HidHotkeys_Init
 *
 * @brief   Initialization function for the HidHotkeys App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void HidHotkeys_Init( uint8 task_id )
{
  HidHotkeysTaskId = task_id;

  // Setup the GAP
  VOID GAP_SetParamValue( TGAP_CONN_PAUSE_PERIPHERAL, DEFAULT_CONN_PAUSE_PERIPHERAL );
  
  // Setup the GAP Peripheral Role Profile
  {
    uint8 initial_advertising_enable = FALSE;

    // By setting this to zero, the device will go into the waiting state after
    // being discoverable for 30.72 second, and will not being advertising again
    // until the enabler is set back to TRUE
    uint16 gapRole_AdvertOffTime = 0;

    uint8 enable_update_request = DEFAULT_ENABLE_UPDATE_REQUEST;
    uint16 desired_min_interval = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
    uint16 desired_max_interval = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
    uint16 desired_slave_latency = DEFAULT_DESIRED_SLAVE_LATENCY;
    uint16 desired_conn_timeout = DEFAULT_DESIRED_CONN_TIMEOUT;

    // Set the GAP Role Parameters
    GAPRole_SetParameter( GAPROLE_ADVERT_ENABLED, sizeof( uint8 ), &initial_advertising_enable );
    GAPRole_SetParameter( GAPROLE_ADVERT_OFF_TIME, sizeof( uint16 ), &gapRole_AdvertOffTime );

    GAPRole_SetParameter( GAPROLE_ADVERT_DATA, sizeof( advData ), advData );
    GAPRole_SetParameter( GAPROLE_SCAN_RSP_DATA, sizeof ( scanData ), scanData );

    GAPRole_SetParameter( GAPROLE_PARAM_UPDATE_ENABLE, sizeof( uint8 ), &enable_update_request );
    GAPRole_SetParameter( GAPROLE_MIN_CONN_INTERVAL, sizeof( uint16 ), &desired_min_interval );
    GAPRole_SetParameter( GAPROLE_MAX_CONN_INTERVAL, sizeof( uint16 ), &desired_max_interval );
    GAPRole_SetParameter( GAPROLE_SLAVE_LATENCY, sizeof( uint16 ), &desired_slave_latency );
    GAPRole_SetParameter( GAPROLE_TIMEOUT_MULTIPLIER, sizeof( uint16 ), &desired_conn_timeout );
  }

  // Set the GAP Characteristics
  GGS_SetParameter( GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, (void *) attDeviceName );

  // Setup the GAP Bond Manager
  {
    uint32 passkey = DEFAULT_PASSCODE;
    uint8 pairMode = DEFAULT_PAIRING_MODE;
    uint8 mitm = DEFAULT_MITM_MODE;
    uint8 ioCap = DEFAULT_IO_CAPABILITIES;
    uint8 bonding = DEFAULT_BONDING_MODE;
    GAPBondMgr_SetParameter( GAPBOND_DEFAULT_PASSCODE, sizeof( uint32 ), &passkey );
    GAPBondMgr_SetParameter( GAPBOND_PAIRING_MODE, sizeof( uint8 ), &pairMode );
    GAPBondMgr_SetParameter( GAPBOND_MITM_PROTECTION, sizeof( uint8 ), &mitm );
    GAPBondMgr_SetParameter( GAPBOND_IO_CAPABILITIES, sizeof( uint8 ), &ioCap );
    GAPBondMgr_SetParameter( GAPBOND_BONDING_ENABLED, sizeof( uint8 ), &bonding );
  }

  // Setup Battery Characteristic Values
  {
    uint8 critical = DEFAULT_BATT_CRITICAL_LEVEL;
    
    Batt_SetParameter( BATT_PARAM_CRITICAL_LEVEL, sizeof (uint8), &critical );

    Batt_Setup( HAL_ADC_CHN_AIN6, BATT_ADC_LEVEL_2_0V, BATT_ADC_LEVEL_3_0V,
                HidHotkeysBattMeasSetupCB, HidHotkeysBattMeasTeardownCB, NULL );

    /* Control of transistor in the battery measurement voltage divider */
    P1DIR |= (1<<5);
    P1SEL &= ~(1<<5);
    P1_5 = 0; // Stop feeding current through voltage divider
    
    /* Analog measuring pin setup. This will be retained during ADC read */
    P0DIR |= (1<<6);
    P0SEL &= ~(1<<6);
    P0_6 = 0; // LOW output to stop leakage current due to default pull-up
  }

  // Set up HID keyboard service
  HidKbd_AddService( );

  // Register for HID Dev callback
  HidDev_Register( &HidHotkeysCfg, &HidHotkeysHidCBs );

  // Register for all key events - This app will handle all key events
  RegisterForKeys( HidHotkeysTaskId );

#if defined( CC2540_MINIDK )
  // makes sure LEDs are off
  HalLedSet( (HAL_LED_1 | HAL_LED_2), HAL_LED_MODE_OFF );

  // For keyfob board set GPIO pins into a power-optimized state
  // Note that there is still some leakage current from the buzzer,
  // accelerometer, LEDs, and buttons on the PCB.

  P0SEL = 0; // Configure Port 0 as GPIO
  P1SEL = 0; // Configure Port 1 as GPIO
  P2SEL = 0; // Configure Port 2 as GPIO

  P0DIR = 0xFC; // Port 0 pins P0.0 and P0.1 as input (buttons),
                // all others (P0.2-P0.7) as output
  P1DIR = 0xFF; // All port 1 pins (P1.0-P1.7) as output
  P2DIR = 0x1F; // All port 1 pins (P2.0-P2.4) as output

  P0 = 0x03; // All pins on port 0 to low except for P0.0 and P0.1 (buttons)
  P1 = 0;   // All pins on port 1 to low
  P2 = 0;   // All pins on port 2 to low

#endif // #if defined( CC2540_MINIDK )

  // Setup a delayed profile startup
  osal_set_event( HidHotkeysTaskId, START_DEVICE_EVT );
}

/*********************************************************************
 * @fn      HidHotkeys_ProcessEvent
 *
 * @brief   HidHotkeys Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16 HidHotkeys_ProcessEvent( uint8 task_id, uint16 events )
{

  VOID task_id; // OSAL required parameter that isn't used in this function

  if ( events & SYS_EVENT_MSG )
  {
    uint8 *pMsg;

    if ( (pMsg = osal_msg_receive( HidHotkeysTaskId )) != NULL )
    {
      HidHotkeys_ProcessOSALMsg( (osal_event_hdr_t *)pMsg );

      // Release the OSAL message
      VOID osal_msg_deallocate( pMsg );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

  if ( events & START_DEVICE_EVT )
  {
    return ( events ^ START_DEVICE_EVT );
  }

   return 0;
}

/*********************************************************************
 * @fn      HidHotkeys_ProcessOSALMsg
 *
 * @brief   Process an incoming task message.
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
static void HidHotkeys_ProcessOSALMsg( osal_event_hdr_t *pMsg )
{
  switch ( pMsg->event )
  {
    case KEY_CHANGE:
      HidHotkeys_HandleKeys( ((keyChange_t *)pMsg)->state, ((keyChange_t *)pMsg)->keys );
      break;

    case GATT_MSG_EVENT:
      HidHotkeys_ProcessGattMsg( (gattMsgEvent_t *)pMsg );
      break;
      
    default:
      break;
  }
}

/*********************************************************************
 * @fn      HidHotkeys_HandleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_2
 *                 HAL_KEY_SW_1
 *
 * @return  none
 */
static void HidHotkeys_HandleKeys( uint8 shift, uint8 keys )
{
  static uint8 prevKey1 = 0;
  static uint8 prevKey2 = 0;
  static uint8 prevKey3 = 0;
#ifdef SCAN_PARAM_NOTIFY_TEST
  static uint8 prevKey4 = 0;
#endif // SCAN_PARAM_NOTIFY_TEST

  (void)shift;  // Intentionally unreferenced parameter

  if ( (keys & HAL_KEY_SW_1) && (prevKey1 == 0) )
  {
    // pressed
    HidHotkeysSendReport( KEY_LEFT_ARROW );
    prevKey1 = 1;
  }
  else if ( !(keys & HAL_KEY_SW_1) && (prevKey1 == 1) )
  {
    // released
    HidHotkeysSendReport( KEY_NONE );
    prevKey1 = 0;
  }

  if ( (keys & HAL_KEY_SW_2) && (prevKey2 == 0) )
  {
    // pressed
    HidHotkeysSendReport( KEY_RIGHT_ARROW );
    prevKey2 = 1;
  }
  else if ( !(keys & HAL_KEY_SW_2) && (prevKey2 == 1) )
  {
    // released
    HidHotkeysSendReport( KEY_NONE );
    prevKey2 = 0;
  }
  
  if ( (keys & HAL_KEY_SW_5) && (prevKey3 == 0) )
  {
    // pressed
    if ( hidBootMouseEnabled )
    {
      HidHotkeysSendMouseReport( MOUSE_BUTTON_1 );
    }
    prevKey3 = 1;
  }
  else if ( !(keys & HAL_KEY_SW_5) && (prevKey3 == 1) )
  {
    // released
    if ( hidBootMouseEnabled )
    {
      HidHotkeysSendMouseReport( MOUSE_BUTTON_NONE );
    }
    prevKey3 = 0;
  }

#ifdef SCAN_PARAM_NOTIFY_TEST
  if ( (keys & HAL_KEY_SW_3) && (prevKey4 == 0) )
  {
    uint16 connHandle;
    // pressed
    // get connection handle
    GAPRole_GetParameter( GAPROLE_CONNHANDLE, &connHandle );

    ScanParam_RefreshNotify(connHandle);
    prevKey4 = 1;
  }
  else if ( !(keys & HAL_KEY_SW_5) && (prevKey3 == 1) )
  {
    // released
    prevKey4 = 0;
  }
#endif // SCAN_PARAM_NOTIFY_TEST
}

/*********************************************************************
 * @fn      HidHotkeys_ProcessGattMsg
 *
 * @brief   Process GATT messages
 *
 * @return  none
 */
static void HidHotkeys_ProcessGattMsg( gattMsgEvent_t *pMsg )
{
  GATT_bm_free( &pMsg->msg, pMsg->method );
}

/*********************************************************************
 * @fn      HidHotkeysSendReport
 *
 * @brief   Build and send a HID keyboard report.
 *
 * @param   keycode - HID keycode.
 *
 * @return  none
 */
static void HidHotkeysSendReport( uint8 keycode )
{
  uint8 buf[HID_KEYBOARD_IN_RPT_LEN];

  buf[0] = 0;         // Modifier keys
  buf[1] = 0;         // Reserved
  buf[2] = keycode;   // Keycode 1
  buf[3] = 0;         // Keycode 2
  buf[4] = 0;         // Keycode 3
  buf[5] = 0;         // Keycode 4
  buf[6] = 0;         // Keycode 5
  buf[7] = 0;         // Keycode 6

  HidDev_Report( HID_RPT_ID_KEY_IN, HID_REPORT_TYPE_INPUT,
                HID_KEYBOARD_IN_RPT_LEN, buf );
}

/*********************************************************************
 * @fn      HidHotkeysSendMouseReport
 *
 * @brief   Build and send a HID mouse report.
 *
 * @param   buttons - Mouse button code
 *
 * @return  none
 */
static void HidHotkeysSendMouseReport( uint8 buttons )
{
  uint8 buf[HID_MOUSE_IN_RPT_LEN];

  buf[0] = buttons;   // Buttons
  buf[1] = 0;         // X
  buf[2] = 0;         // Y
  buf[3] = 0;         // Wheel
  buf[4] = 0;         // AC Pan

  HidDev_Report( HID_RPT_ID_MOUSE_IN, HID_REPORT_TYPE_INPUT,
                 HID_MOUSE_IN_RPT_LEN, buf );
}

/*********************************************************************
 * @fn      HidHotkeysRcvReport
 *
 * @brief   Process an incoming HID keyboard report.
 *
 * @param   len - Length of report.
 * @param   pData - Report data.
 *
 * @return  status
 */
static uint8 HidHotkeysRcvReport( uint8 len, uint8 *pData )
{
  // verify data length
  if ( len == HID_LED_OUT_RPT_LEN )
  {
    // set keyfob LEDs
    HalLedSet( HAL_LED_1, ((*pData & LED_CAPS_LOCK) == LED_CAPS_LOCK) );
    HalLedSet( HAL_LED_2, ((*pData & LED_NUM_LOCK) == LED_NUM_LOCK) );

    return SUCCESS;
  }
  else
  {
    return ATT_ERR_INVALID_VALUE_SIZE;
  }
}

/*********************************************************************
 * @fn      HidHotkeysRptCB
 *
 * @brief   HID Dev report callback.
 *
 * @param   id - HID report ID.
 * @param   type - HID report type.
 * @param   uuid - attribute uuid.
 * @param   oper - operation:  read, write, etc.
 * @param   len - Length of report.
 * @param   pData - Report data.
 *
 * @return  GATT status code.
 */
static uint8 HidHotkeysRptCB( uint8 id, uint8 type, uint16 uuid,
                             uint8 oper, uint8 *pLen, uint8 *pData )
{
  uint8 status = SUCCESS;

  // write
  if ( oper == HID_DEV_OPER_WRITE )
  {
    if ( uuid == REPORT_UUID )
    {
      // process write to LED output report; ignore others
      if ( type == HID_REPORT_TYPE_OUTPUT )
      {
        status = HidHotkeysRcvReport( *pLen, pData );
      }
    }

    if ( status == SUCCESS )
    {
      status = HidKbd_SetParameter( id, type, uuid, *pLen, pData );
    }
  }
  // read
  else if ( oper == HID_DEV_OPER_READ )
  {
    status = HidKbd_GetParameter( id, type, uuid, pLen, pData );
  }
  // notifications enabled
  else if ( oper == HID_DEV_OPER_ENABLE )
  {
    if ( id == HID_RPT_ID_MOUSE_IN && type == HID_REPORT_TYPE_INPUT )
    {
      hidBootMouseEnabled = TRUE;
    }
  }
  // notifications disabled
  else if ( oper == HID_DEV_OPER_DISABLE )
  {
    if ( id == HID_RPT_ID_MOUSE_IN && type == HID_REPORT_TYPE_INPUT )
    {
      hidBootMouseEnabled = FALSE;
    }
  }

  return status;
}

/*********************************************************************
 * @fn      HidHotkeysEvtCB
 *
 * @brief   HID Dev event callback.
 *
 * @param   evt - event ID.
 *
 * @return  HID response code.
 */
static void HidHotkeysEvtCB( uint8 evt )
{
  // process enter/exit suspend or enter/exit boot mode

  return;
}

/*********************************************************************
 * @fn      HidHotkeysBattMeasSetupCB
 *
 * @brief   Callback from battery service to set up measurement environment
 *
 * @param   none
 *
 * @return  none
 */
static void HidHotkeysBattMeasSetupCB( void )
{
  P1_5 = 1;  // Open transistor in voltage divider
  
  /* Analog pin P0.6 is configured by HAL ADC routine */
}

/*********************************************************************
 * @fn      HidHotkeysBattMeasTeardownCB
 *
 * @brief   Callback from battery service to tear down measurement environment
 *
 * @param   none
 *
 * @return  none
 */
static void HidHotkeysBattMeasTeardownCB( void )
{
  P1_5 = 0; // Stop feeding current through voltage divider
  
  /* Idle state P0.6 settings are retained from Init */
}

/*********************************************************************
*********************************************************************/
