/* usb_desc.h — standard USB structures + our device/config/HID descriptors */
#ifndef USB_DESC_H
#define USB_DESC_H
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

/* bRequest values we handle */
#define USB_REQ_GET_STATUS        0
#define USB_REQ_CLEAR_FEATURE     1
#define USB_REQ_SET_FEATURE       3
#define USB_REQ_SET_ADDRESS       5
#define USB_REQ_GET_DESCRIPTOR    6
#define USB_REQ_SET_DESCRIPTOR    7
#define USB_REQ_GET_CONFIGURATION 8
#define USB_REQ_SET_CONFIGURATION 9
#define USB_REQ_SET_IDLE          0x0A /* HID class request */

#define USB_DESC_TYPE_DEVICE        1
#define USB_DESC_TYPE_CONFIGURATION 2
#define USB_DESC_TYPE_STRING        3
#define USB_DESC_TYPE_INTERFACE     4
#define USB_DESC_TYPE_ENDPOINT      5
#define USB_DESC_TYPE_HID           0x21
#define USB_DESC_TYPE_HID_REPORT    0x22

/* ---- Device descriptor (18 bytes) ---- */
static const uint8_t usb_device_descriptor[18] = {
    18,                     /* bLength */
    USB_DESC_TYPE_DEVICE,   /* bDescriptorType */
    0x00, 0x02,             /* bcdUSB = 2.00 */
    0x00,                   /* bDeviceClass (defined at interface level) */
    0x00,                   /* bDeviceSubClass */
    0x00,                   /* bDeviceProtocol */
    64,                     /* bMaxPacketSize0 */
    0x83, 0x04,             /* idVendor  = 0x0483 (STMicro's own VID, fine for hobby use) */
    0x11, 0x57,             /* idProduct = 0x5711 (arbitrary) */
    0x00, 0x01,             /* bcdDevice = 1.00 */
    1,                      /* iManufacturer -> string 1 */
    2,                      /* iProduct -> string 2 */
    0,                      /* iSerialNumber -> none */
    1,                      /* bNumConfigurations */
};

/* ---- HID keyboard report descriptor (standard 6-key rollover boot-protocol layout) ---- */
static const uint8_t hid_report_descriptor[] = {
    0x05, 0x01,       /* Usage Page (Generic Desktop) */
    0x09, 0x06,       /* Usage (Keyboard) */
    0xA1, 0x01,       /* Collection (Application) */
    0x05, 0x07,       /*   Usage Page (Key Codes) */
    0x19, 0xE0,       /*   Usage Minimum (224) */
    0x29, 0xE7,       /*   Usage Maximum (231) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x25, 0x01,       /*   Logical Maximum (1) */
    0x75, 0x01,       /*   Report Size (1) */
    0x95, 0x08,       /*   Report Count (8) -- modifier byte */
    0x81, 0x02,       /*   Input (Data,Var,Abs)  -> modifier bits */
    0x95, 0x01,       /*   Report Count (1) */
    0x75, 0x08,       /*   Report Size (8) */
    0x81, 0x01,       /*   Input (Const)         -> reserved byte */
    0x95, 0x06,       /*   Report Count (6) */
    0x75, 0x08,       /*   Report Size (8) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x25, 0x65,       /*   Logical Maximum (101) */
    0x05, 0x07,       /*   Usage Page (Key Codes) */
    0x19, 0x00,       /*   Usage Minimum (0) */
    0x29, 0x65,       /*   Usage Maximum (101) */
    0x81, 0x00,       /*   Input (Data,Array)    -> 6 keycode bytes */
    0xC0              /* End Collection */
};
#define HID_REPORT_DESC_LEN (sizeof(hid_report_descriptor))

/* ---- Configuration descriptor: Config + Interface + HID + Endpoint (34 bytes total) ---- */
static const uint8_t usb_config_descriptor[34] = {
    /* Configuration descriptor (9) */
    9, USB_DESC_TYPE_CONFIGURATION,
    34, 0x00,               /* wTotalLength = 34 */
    1,                       /* bNumInterfaces */
    1,                       /* bConfigurationValue */
    0,                       /* iConfiguration */
    0x80,                    /* bmAttributes: bus powered */
    50,                      /* bMaxPower = 100mA */

    /* Interface descriptor (9) */
    9, USB_DESC_TYPE_INTERFACE,
    0,                       /* bInterfaceNumber */
    0,                       /* bAlternateSetting */
    1,                       /* bNumEndpoints */
    0x03,                    /* bInterfaceClass = HID */
    0x01,                    /* bInterfaceSubClass = Boot */
    0x01,                    /* bInterfaceProtocol = Keyboard */
    0,                       /* iInterface */

    /* HID descriptor (9) */
    9, USB_DESC_TYPE_HID,
    0x11, 0x01,              /* bcdHID = 1.11 */
    0,                       /* bCountryCode */
    1,                       /* bNumDescriptors */
    USB_DESC_TYPE_HID_REPORT,
    (uint8_t)(HID_REPORT_DESC_LEN & 0xFF),
    (uint8_t)(HID_REPORT_DESC_LEN >> 8),

    /* Endpoint descriptor (7): EP1 IN, interrupt, 8 bytes, 10ms interval */
    7, USB_DESC_TYPE_ENDPOINT,
    0x81,                    /* bEndpointAddress: IN, EP1 */
    0x03,                    /* bmAttributes: Interrupt */
    8, 0x00,                 /* wMaxPacketSize = 8 */
    10,                      /* bInterval (ms, full speed) */
};

/* ---- String descriptors (UTF-16LE) ---- */
static const uint8_t usb_str_lang[4]     = {4, USB_DESC_TYPE_STRING, 0x09, 0x04}; /* en-US */
static const uint8_t usb_str_manuf[]     = {20, USB_DESC_TYPE_STRING,
    'B',0,'a',0,'r',0,'e',0,'M',0,'e',0,'t',0,'a',0,'l',0};
static const uint8_t usb_str_product[]   = {26, USB_DESC_TYPE_STRING,
    'B',0,'l',0,'a',0,'c',0,'k',0,'P',0,'i',0,'l',0,'l',0,' ',0,'K',0,'B',0};

#endif
