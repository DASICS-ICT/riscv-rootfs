#ifndef _UATTR_H_
#define _UATTR_H_

#define ATTR_UMAIN_TEXT __attribute__((section(".umaintext"),aligned(8)))
#define ATTR_UMAIN_DATA __attribute__((section(".umaindata"),aligned(8)))

#define ATTR_ULIB_TEXT __attribute__((section(".ulibtext"),aligned(8)))
#define ATTR_ULIB_DATA __attribute__((section(".ulibdata"),aligned(8)))

#define ATTR_UFREEZONE_TEXT __attribute__((section(".ufreezonetext"),aligned(8)))
#define ATTR_UFREEZONE_DATA __attribute__((section(".ufreezonedata"),aligned(8)))

#endif