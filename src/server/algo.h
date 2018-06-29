#ifndef _ALGORITHM_HEADER_H_
#define _ALGORITHM_HEADER_H_

namespace algo
{
        /*****************************************************************
         * 函数功能：整型数的快速排序算法
         ******************************************************************/
        void QuickSort (long *arr, int low, int high);

        /***********************************************************************************
         * 函数名：FindingString
         * 功  能：在lpszSour中查找字符串lpszFind，lpszFind中可以包含通配字符‘?’
         * 参  数：nStart为在lpszSour中的起始查找位置
         * 返回值：成功返回匹配位置，否则返回-1
         * 注  意：Called by “bool MatchingString()”
         **********************************************************************************/
        int FindingString (const char* lpszSour, const char* lpszFind, int nStart  = 0 );

        /*****************************************************************************************
         * 函数名：MatchingString
         * 功	  能：带通配符的字符串匹配
         * 参	  数：lpszSour是一个普通字符串；
         *			     lpszMatch是一可以包含通配符的字符串；
         *			     bMatchCase为0，不区分大小写，否则区分大小写。
         * 返  回  值：匹配，返回1；否则返回0。
         * 通配符意义：
         *		‘*’	代表任意字符串，包括空字符串；
         *		‘?’	代表任意一个字符，不能为空；
         ********************************************************************************************/
        bool MatchingString (const char* lpszSour, const char* lpszMatch, bool bMatchCase = false);

        /***********************************************************************************
         * 函数名：MultiMatching
         * 功  能：多重匹配，不同匹配字符串之间用‘,’隔开
         * 例  如：“*.h,*.cpp”将依次匹配“*.h”和“*.cpp”
         * 参  数：nMatchLogic = 0, 不同匹配求或，else求与；bMatchCase, 是否大小敏感
         * 返回值：如果bRetReversed = 0, 匹配返回true；否则不匹配返回true
         *************************************************************************************/
        bool MultiMatching (const char* lpszSour, const char* lpszMatch, int nMatchLogic = 0, bool bRetReversed = false, bool bMatchCase = false);

}

#endif/* _ALGORITHM_HEADER_H_ */