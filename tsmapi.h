#ifndef _TSMAPI_H_
#define _TSMAPI_H_
void TSM_Install(int rate);
int TSM_NewService(void(*function)(void), int rate, int priority, int pause);
void TSM_DelService(int id);
void TSM_Remove(void);
#endif
