/* pointer.h — AINE pointer/mouse input API */
#ifndef AINE_POINTER_H
#define AINE_POINTER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Start monitoring mouse events for the given NSWindow* (passed as void*
 * for C compatibility). Pass NULL to monitor all windows.
 */
void aine_input_pointer_start(void *ns_window);
void aine_input_pointer_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* AINE_POINTER_H */
