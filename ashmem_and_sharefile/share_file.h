#ifndef _SHARE_FILE_H_
#define _SHARE_FILE_H_

#include <linux/ioctl.h>

#define SHFILE_SHARE_FD        _IOW('s', 1, int)
#define SHFILE_GET_FD          _IOR('s', 1, int)


#endif /* _SHARE_FILE_H_ */
