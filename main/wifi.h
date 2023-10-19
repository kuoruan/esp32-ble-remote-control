#ifndef __WIFI_H__
#define __WIFI_H__

void wifi_init();

void wifi_connect(void *on_connected, void *on_disconnected);

#endif
