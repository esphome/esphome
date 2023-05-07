/**
 * Author: SeeedStudio
 * Link:
 * Accessed: 5/7/2023
 * License: MIT
 */
#ifndef _RADAR_H__
#define _RADAR_H__

class radar
{
    private:
    public:
        int Bodysign_val(int ad1, int ad2, int ad3, int ad4, int ad5);
        int Situation_judgment(int ad1, int ad2, int ad3, int ad4, int ad5);
        int Fall_judgment(int ad1, int ad2, int ad3, int ad4);
        char CRC(char ad1, char ad2, char ad3, char ad4, char ad5, char ad6, char ad7);
        static unsigned short int us_CalculateCrc16(unsigned char *lpuc_Frame, unsigned short int lus_Len);
};

#endif
/** END of copied and changed content */