/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _MAINCALL_SERVICE_UTMOD_H
#define _MAINCALL_SERVICE_UTMOD_H

int maincall_service_valid_service(void);
int maincall_service_full_registers(void);
int maincall_service_unknown_service(void);
int maincall_service_bad_arguments(void);
int maincall_service_invalid_source(void);
int maincall_service_untrusted_stack(void);

#endif
