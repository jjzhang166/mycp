/*
    MYCP is a HTTP and C++ Web Application Server.
    Copyright (C) 2009-2010  Akee Yang <akee.yang@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef WIN32
#pragma warning(disable:4996)
#include <windows.h>
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
#endif // WIN32

#include <string.h>
#include <stdarg.h>
#include <algorithm>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>
//#include <CGCBase/app.h>
#include <CGCBase/cgcService.h>
#include "md5.h"
#include "Base64.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/locale/encoding.hpp>
//#include <boost/foreach.hpp>
using namespace boost::property_tree;
using namespace boost::locale;

// cgc head
#include "cgcString.h"
using namespace cgc;
extern "C"
{
#include "d3des.h"
}
// ECB
void DesEncrypt(const char* szKey, unsigned char* lpbySrc, unsigned char* lpbyDest, int iLength)
{
	unsigned char byKey[8] = {0};
	char szTempKey[128];
	strcpy(szTempKey, szKey);
	makekey(szTempKey, byKey);

	deskey(byKey, EN0);
	for (int i = 0; i < iLength; i += 8)
		des(lpbySrc + i, lpbyDest + i);
}

void DesDecrypt(const char* szKey, unsigned char* lpbySrc, unsigned char* lpbyDest, int iLength)
{
	unsigned char byKey[8] = {0};
	char szTempKey[128];
	strcpy(szTempKey, szKey);
	makekey(szTempKey, byKey);

	deskey(byKey, DE1);
	for (int i = 0; i < iLength; i += 8)
		des(lpbySrc + i, lpbyDest + i);
}
//
////////////////////////////////////////////////////////////////////////////
//
//// initial permutation IP
//const static char IP_Table[64] = {
// 58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4,
// 62, 54, 46, 38, 30, 22, 14, 6, 64, 56, 48, 40, 32, 24, 16, 8,
// 57, 49, 41, 33, 25, 17,  9, 1, 59, 51, 43, 35, 27, 19, 11, 3,
//    61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7
//};
//// final permutation IP^-1
//const static char IPR_Table[64] = {
// 40, 8, 48, 16, 56, 24, 64, 32, 39, 7, 47, 15, 55, 23, 63, 31,
// 38, 6, 46, 14, 54, 22, 62, 30, 37, 5, 45, 13, 53, 21, 61, 29,
//    36, 4, 44, 12, 52, 20, 60, 28, 35, 3, 43, 11, 51, 19, 59, 27,
// 34, 2, 42, 10, 50, 18, 58, 26, 33, 1, 41,  9, 49, 17, 57, 25
//};
//// expansion operation matrix
//static const char E_Table[48] = {
// 32,  1,  2,  3,  4,  5,  4,  5,  6,  7,  8,  9,
//  8,  9, 10, 11, 12, 13, 12, 13, 14, 15, 16, 17,
// 16, 17, 18, 19, 20, 21, 20, 21, 22, 23, 24, 25,
// 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32,  1
//};
//// 32-bit permutation function P used on the output of the S-boxes
//const static char P_Table[32] = {
// 16, 7, 20, 21, 29, 12, 28, 17, 1,  15, 23, 26, 5,  18, 31, 10,
// 2,  8, 24, 14, 32, 27, 3,  9,  19, 13, 30, 6,  22, 11, 4,  25
//};
//// permuted choice table (key)
//const static char PC1_Table[56] = {
// 57, 49, 41, 33, 25, 17,  9,  1, 58, 50, 42, 34, 26, 18,
// 10,  2, 59, 51, 43, 35, 27, 19, 11,  3, 60, 52, 44, 36,
// 63, 55, 47, 39, 31, 23, 15,  7, 62, 54, 46, 38, 30, 22,
// 14,  6, 61, 53, 45, 37, 29, 21, 13,  5, 28, 20, 12,  4
//};
//// permuted choice key (table)
//const static char PC2_Table[48] = {
// 14, 17, 11, 24,  1,  5,  3, 28, 15,  6, 21, 10,
// 23, 19, 12,  4, 26,  8, 16,  7, 27, 20, 13,  2,
// 41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
// 44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32
//};
//// number left rotations of pc1
//const static char LOOP_Table[16] = {
// 1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1
//};
//// The (in)famous S-boxes
//const static char S_Box[8][4][16] = {
// // S1
// 14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,
//  0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8,
//  4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,
//    15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13,
// // S2
//    15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,
//  3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5,
//  0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,
//    13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9,
// // S3
//    10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,
// 13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1,
// 13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,
//     1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12,
// // S4
//     7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,
// 13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9,
// 10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,
//     3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14,
// // S5
//     2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,
// 14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6,
//  4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,
//    11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3,
// // S6
//    12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,
// 10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8,
//  9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,
//     4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13,
// // S7
//     4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,
// 13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6,
//  1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,
//     6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12,
// // S8
//    13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,
//  1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2,
//  7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,
//     2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11
//};
//
////////////////////////////////////////////////////////////////////////////
//
//typedef bool    (*PSubKey)[16][48];
//
////////////////////////////////////////////////////////////////////////////
//
//static void DES(char Out[8], char In[8], const PSubKey pSubKey, bool Type);//标准DES加/解密
//static void SetKey(const char* Key, int len);// 设置密钥
//static void SetSubKey(PSubKey pSubKey, const char Key[8]);// 设置子密钥
//static void F_func(bool In[32], const bool Ki[48]);// f 函数
//static void S_func(bool Out[32], const bool In[48]);// S 盒代替
//static void Transform(bool *Out, bool *In, const char *Table, int len);// 变换
//static void Xor(bool *InA, const bool *InB, int len);// 异或
//static void RotateL(bool *In, int len, int loop);// 循环左移
//static void ByteToBit(bool *Out, const char *In, int bits);// 字节组转换成位组
//static void BitToByte(char *Out, const bool *In, int bits);// 位组转换成字节组
//
////////////////////////////////////////////////////////////////////////////
//
//static bool SubKey[2][16][48];// 16圈子密钥
//static bool Is3DES;// 3次DES标志
//static char Tmp[256], deskey[16];
//
////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////
//// Code starts from Line 130
////////////////////////////////////////////////////////////////////////////
//bool Des_Go(char *Out, char *In, long datalen, const char *Key, int keylen, bool Type)
//{
//    if( !( Out && In && Key && (datalen=(datalen+7)&0xfffffff8) ) )
//  return false;
// SetKey(Key, keylen);
// if( !Is3DES ) {   // 1次DES
//  for(long i=0,j=datalen>>3; i<j; ++i,Out+=8,In+=8)
//   DES(Out, In, &SubKey[0], Type);
// } else{   // 3次DES 加密:加(key0)-解(key1)-加(key0) 解密::解(key0)-加(key1)-解(key0)
//  for(long i=0,j=datalen>>3; i<j; ++i,Out+=8,In+=8) {
//   DES(Out, In,  &SubKey[0], Type);
//   DES(Out, Out, &SubKey[1], !Type);
//   DES(Out, Out, &SubKey[0], Type);
//  }
// }
// return true;
//}
//void SetKey(const char* Key, int len)
//{
// memset(deskey, 0, 16);
// memcpy(deskey, Key, len>16?16:len);
// SetSubKey(&SubKey[0], &deskey[0]);
// Is3DES = len>8 ? (SetSubKey(&SubKey[1], &deskey[8]), true) : false;
//}
////#define ENCRYPT 1
//void DES(char Out[8], char In[8], const PSubKey pSubKey, bool Type)
//{
//    static bool M[64], tmp[32], *Li=&M[0], *Ri=&M[32];
//    ByteToBit(M, In, 64);
//    Transform(M, M, IP_Table, 64);
//    if( Type == true ){	// ENCRYPT
//        for(int i=0; i<16; ++i) {
//            memcpy(tmp, Ri, 32);
//            F_func(Ri, (*pSubKey)[i]);
//            Xor(Ri, Li, 32);
//            memcpy(Li, tmp, 32);
//        }
//    }else{
//        for(int i=15; i>=0; --i) {
//            memcpy(tmp, Li, 32);
//            F_func(Li, (*pSubKey)[i]);
//            Xor(Li, Ri, 32);
//            memcpy(Ri, tmp, 32);
//        }
// }
//    Transform(M, M, IPR_Table, 64);
//    BitToByte(Out, M, 64);
//}
//void SetSubKey(PSubKey pSubKey, const char Key[8])
//{
//    static bool K[64], *KL=&K[0], *KR=&K[28];
//    ByteToBit(K, Key, 64);
//    Transform(K, K, PC1_Table, 56);
//    for(int i=0; i<16; ++i) {
//        RotateL(KL, 28, LOOP_Table[i]);
//        RotateL(KR, 28, LOOP_Table[i]);
//        Transform((*pSubKey)[i], K, PC2_Table, 48);
//    }
//}
//void F_func(bool In[32], const bool Ki[48])
//{
//    static bool MR[48];
//    Transform(MR, In, E_Table, 48);
//    Xor(MR, Ki, 48);
//    S_func(In, MR);
//    Transform(In, In, P_Table, 32);
//}
//void S_func(bool Out[32], const bool In[48])
//{
//    for(char i=0,j,k; i<8; ++i,In+=6,Out+=4) {
//        j = (In[0]<<1) + In[5];
//        k = (In[1]<<3) + (In[2]<<2) + (In[3]<<1) + In[4];
//  ByteToBit(Out, &S_Box[i][j][k], 4);
//    }
//}
//void Transform(bool *Out, bool *In, const char *Table, int len)
//{
//    for(int i=0; i<len; ++i)
//        Tmp[i] = In[ Table[i]-1 ];
//    memcpy(Out, Tmp, len);
//}
//void Xor(bool *InA, const bool *InB, int len)
//{
//    for(int i=0; i<len; ++i)
//        InA[i] ^= InB[i];
//}
//void RotateL(bool *In, int len, int loop)
//{
//    memcpy(Tmp, In, loop);
//    memcpy(In, In+loop, len-loop);
//    memcpy(In+len-loop, Tmp, loop);
//}
//void ByteToBit(bool *Out, const char *In, int bits)
//{
//    for(int i=0; i<bits; ++i)
//        Out[i] = (In[i>>3]>>(i&7)) & 1;
//}
//void BitToByte(char *Out, const bool *In, int bits)
//{
//    memset(Out, 0, bits>>3);
//    for(int i=0; i<bits; ++i)
//        Out[i>>3] |= In[i]<<(i&7);
//}
////////////////////////////////////////////////////////////////////////////
//// Code ends at Line 231
////////////////////////////////////////////////////////////////////////////

class CStringService
	: public cgcString
{
public:
	typedef boost::shared_ptr<CStringService> pointer;

	static CStringService::pointer create(void)
	{
		return CStringService::pointer(new CStringService());
	}

	virtual tstring serviceName(void) const {return _T("STRINGSERVICE");}

protected:
	virtual bool callService(const tstring& function, const cgcValueInfo::pointer& inParam, cgcValueInfo::pointer outParam)
	{
		if (function == "md5")
		{
			if (inParam.get() == NULL || outParam.get() == NULL) return false;

			const tstring & sInString = inParam->getStr();
			MD5 md5;
			md5.update((const unsigned char*)sInString.c_str(), (unsigned int)sInString.size());
			md5.finalize();
			outParam->totype(cgcValueInfo::TYPE_STRING);
			outParam->setStr(std::string((const char*)md5.hex_digest()));
		//}else if (function == "tohex")
		//{
		//	if (inParam.get() == NULL) return false;

		//	const tstring & sInString = inParam->getStr();
		//	tstring sOutString = toHex(sInString);
		//	//返回结果
		//	if (outParam.get() == NULL)
		//	{
		//		inParam->setStr(sOutString);
		//	}else
		//	{
		//		outParam->totype(cgcValueInfo::TYPE_STRING);
		//		outParam->setStr(sOutString);
		//	}
		}else if (function == "des_enc")
		{
			if (inParam.get() == NULL || inParam->getType()!=cgcValueInfo::TYPE_VECTOR || inParam->getVector().size()<2) return false;

			tstring sKey = inParam->getVector()[0]->getStr();
			if (sKey.size()>128)
				sKey = sKey.substr(0,128);
			const tstring& sInString = inParam->getVector()[1]->getStr();
			bool bConvert2t16 = true;
			if (inParam->getVector().size()>=3)
				bConvert2t16 = inParam->getVector()[2]->getBoolean();
			//printf("**** key=%s\n",sKey.c_str());
			//printf("**** string=%s\n",sInString.c_str());

			unsigned char * lpszIn = new unsigned char[sInString.size()+8];
			memset(lpszIn,0,sInString.size()+8);
			memcpy(lpszIn,sInString.c_str(),sInString.size());
			unsigned char * lpszOut = new unsigned char[sInString.size()+8];
			memset(lpszOut,0,sInString.size()+8);

			DesEncrypt(sKey.c_str(),lpszIn,lpszOut,sInString.size());
			const size_t nEncLen = ((sInString.size()+7)/8)*8;	// 加密后数据长度；

			std::string sOutString;
			if (bConvert2t16)
				sOutString = convert_2t16((const unsigned char*)lpszOut,nEncLen);
			else
				sOutString = std::string((const char*)lpszOut,nEncLen);

			//{	// test OK
			//	printf("** 2t16=%s\n",sOutString.c_str());
			//	unsigned char * lpszIn2 = new unsigned char[sOutString.size()+8];
			//	size_t nInLen = convert_16t2((const unsigned char*)sOutString.c_str(),sOutString.size(),lpszIn2);
			//	printf("** 16t2=%s\n",lpszIn2);

			//	unsigned char * lpszOut2 = new unsigned char[nInLen+8];
			//	memset(lpszOut2,0,nInLen+8);
			//	DesDecrypt(sKey.c_str(),lpszIn2,lpszOut2,nInLen);
			//	printf("** dec-result=%s\n",lpszOut2);
			//	delete[] lpszOut2;
			//	delete[] lpszIn2;
			//}

			delete[] lpszOut;
			delete[] lpszIn;
			//printf("**** 3des result=%s\n",sOutString.c_str());

			//返回结果
			if (outParam.get() == NULL)
			{
				inParam->totype(cgcValueInfo::TYPE_STRING);
				inParam->setStr(sOutString);
			}else
			{
				outParam->totype(cgcValueInfo::TYPE_STRING);
				outParam->setStr(sOutString);
			}
		}else if (function == "des_dec")
		{
			if (inParam.get() == NULL || inParam->getType()!=cgcValueInfo::TYPE_VECTOR || inParam->getVector().size()<2) return false;

			tstring sKey = inParam->getVector()[0]->getStr();
			if (sKey.size()>128)
				sKey = sKey.substr(0,128);
			const tstring& sInString = inParam->getVector()[1]->getStr();
			bool bConvert16t2 = true;
			if (inParam->getVector().size()>=3)
				bConvert16t2 = inParam->getVector()[2]->getBoolean();
			unsigned char * lpszIn = new unsigned char[sInString.size()+8];
			size_t nInLen = sInString.size();
			if (bConvert16t2)
			{
				nInLen = convert_16t2((const unsigned char*)sInString.c_str(),nInLen,lpszIn);
			}else
			{
				memcpy(lpszIn,sInString.c_str(),nInLen);
			}

			unsigned char * lpszOut = new unsigned char[nInLen+8];
			memset(lpszOut,0,nInLen+8);
			DesDecrypt(sKey.c_str(),lpszIn,lpszOut,nInLen);
			const std::string sOutString((const char*)lpszOut);
			delete[] lpszOut;
			delete[] lpszIn;

			//返回结果
			if (outParam.get() == NULL)
			{
				inParam->setStr(sOutString);
			}else
			{
				outParam->totype(cgcValueInfo::TYPE_STRING);
				outParam->setStr(sOutString);
			}
		}else if (function == "url_enc")
		{
			if (inParam.get() == NULL) return false;

			const tstring & sInString = inParam->getStr();
			std::string sOutString = URLEncode(sInString.c_str());

			//返回结果
			if (outParam.get() == NULL)
			{
				inParam->totype(cgcValueInfo::TYPE_STRING);
				inParam->setStr(sOutString);
			}else
			{
				outParam->totype(cgcValueInfo::TYPE_STRING);
				outParam->setStr(sOutString);
			}
		}else if (function == "url_dec")
		{
			if (inParam.get() == NULL) return false;

			const tstring & sInString = inParam->getStr();
			std::string sOutString = URLDecode(sInString.c_str());

			//返回结果
			if (outParam.get() == NULL)
			{
				inParam->setStr(sOutString);
			}else
			{
				outParam->totype(cgcValueInfo::TYPE_STRING);
				outParam->setStr(sOutString);
			}
		}else if (function == "utf8")
		{
			if (inParam.get() == NULL) return false;

			const tstring & sInString = inParam->getStr();
#ifdef WIN32
			const std::string sOutString = convert(sInString.c_str(), CP_ACP, CP_UTF8);
#else
			const std::string sOutString(sInString);	// ???
#endif
			//返回结果
			if (outParam.get() == NULL)
			{
				inParam->setStr(sOutString);
			}else
			{
				outParam->totype(cgcValueInfo::TYPE_STRING);
				outParam->setStr(sOutString);
			}
		}else if (function == "utf82ansi")
		{
			if (inParam.get() == NULL) return false;

			const tstring & sInString = inParam->getStr();
#ifdef WIN32
			const std::string sOutString = convert(sInString.c_str(), CP_UTF8, CP_ACP);
#else
			const std::string sOutString(sInString);	// ???
#endif
			//返回结果
			if (outParam.get() == NULL)
			{
				inParam->setStr(sOutString);
			}else
			{
				outParam->totype(cgcValueInfo::TYPE_STRING);
				outParam->setStr(sOutString);
			}
		}else if (function == "base64_enc")
		{
			if (inParam.get() == NULL || outParam.get() == NULL) return false;
			const tstring & sInString = inParam->getStr();

			outParam->totype(cgcValueInfo::TYPE_STRING);
			if (sInString.empty())
			{
				outParam->setStr(tstring(_T("")));
			}else
			{
				int size = sInString.size()*1.4+1;
				char * buffer = new char[size];
				unsigned int len = Base64Encode(buffer, (const unsigned char*)sInString.c_str(), (unsigned int)sInString.size());
				if (len > 0)
				{
					outParam->setStr(std::string(buffer, len));
				}else
				{
					outParam->setStr(tstring(_T("")));
				}
				delete[] buffer;
			}
		}else if (function == "base64_dec")
		{
			if (inParam.get() == NULL || outParam.get() == NULL) return false;
			const tstring & sInString = inParam->getStr();

			outParam->totype(cgcValueInfo::TYPE_STRING);
			if (sInString.empty())
			{
				outParam->setStr(tstring(_T("")));
			}else
			{
				int size = sInString.size()*0.8+1;
				char * buffer = new char[size];
				unsigned int len = Base64Decode((unsigned char*)buffer, sInString.c_str());
				if (len > 0)
				{
					outParam->setStr(std::string(buffer, len));
				}else
				{
					outParam->setStr(tstring(_T("")));
				}
				delete[] buffer;
			}
		}else if (function == "upper")
		{
			if (inParam.get() == NULL) return false;
			tstring sInString = inParam->getStr();

			boost::to_upper(sInString);
			if (outParam.get() == NULL)
			{
				inParam->setStr(sInString);
			}else
			{
				outParam->totype(cgcValueInfo::TYPE_STRING);
				outParam->setStr(sInString);
			}
		}else if (function == "lower")
		{
			if (inParam.get() == NULL) return false;
			tstring sInString = inParam->getStr();
			boost::to_lower(sInString);
			if (outParam.get() == NULL)
			{
				inParam->setStr(sInString);
			}else
			{
				outParam->totype(cgcValueInfo::TYPE_STRING);
				outParam->setStr(sInString);
			}
		}else if (function == "parse_httpurl")
		{
			if (inParam.get() == NULL || outParam.get() == NULL) return false;
			const tstring & sInString = inParam->getStr();

			outParam->totype(cgcValueInfo::TYPE_MAP);
			outParam->reset();
			if (!sInString.empty())
			{
				tstring parameter;
				std::string::size_type find = 0;
				do
				{
					// Get [parameter=value]
					tstring::size_type findParameter = sInString.find("&", find+1);
					if (findParameter == std::string::npos)
					{
						parameter = sInString.substr(find, sInString.size()-find);
					}else
					{
						parameter = sInString.substr(find, findParameter-find);
						findParameter += 1;
					}
					find = findParameter;

					// Get parameter/value
					findParameter = parameter.find("=", 1);
					if (findParameter == std::string::npos)
					{
						// ERROR
						break;
					}

					tstring p = parameter.substr(0, findParameter);
					tstring v = parameter.substr(findParameter+1, parameter.size()-findParameter);
					outParam->insertMap(p, CGC_VALUEINFO(v));
				}while (find != std::string::npos);
			}
		}else if (function == "to_json")
		{
			if (inParam.get() == NULL) return false;
			return toJson(inParam,outParam);
		}else if (function == "split_str2list")
		{
			if (inParam.get() == NULL || inParam->getType()!=cgcValueInfo::TYPE_VECTOR || inParam->size()<2) return false;
			const tstring sString = inParam->getVector()[0]->getStr();
			const tstring sInterval = inParam->getVector()[1]->getStr();

			cgcValueInfo::pointer pOutParam = outParam.get() == NULL?inParam:outParam;
			pOutParam->totype(cgcValueInfo::TYPE_VECTOR);
			pOutParam->reset();

			std::vector<std::string> pList;
			SplitString2List(sString.c_str(),sInterval.c_str(),pList);
			for (size_t i=0;i<pList.size();i++)
			{
				pOutParam->addVector(CGC_VALUEINFO(pList[i]));
			}
		}else
		{
			return false;
		}
		//SplitString2List
		return true;
	}

//创建JSON数组：
//boost::property_tree::ptree array;
//array.push_back(std::make_pair("", "element0"));
//array.push_back(std::make_pair("", "element1"));
//boost::property_tree::ptree props;
//props.push_back(std::make_pair("array", array));
//boost::property_tree::write_json("prob.json", props);

	void PutJson(wptree& ptResponse,const tstring& sKey, const cgcValueInfo::pointer& pValueInfo, bool bRoot=false)
	{
		//printf("type=%d,key=%s\n",pValueInfo->getType(),sKey.c_str());
		switch (pValueInfo->getType())
		{
		case cgcValueInfo::TYPE_VALUEINFO:
			{
				wptree ptChild;
				PutJson(ptChild, "", pValueInfo->getValueInfo());
				if (sKey.empty())
					ptResponse.push_back(std::make_pair(L"", ptChild));
				else
					ptResponse.push_back(std::make_pair(conv::utf_to_utf<wchar_t>(sKey), ptChild));
			}break;
		case cgcValueInfo::TYPE_MAP:
			{
				wptree ptChild;
				const CLockMap<tstring, cgcValueInfo::pointer>& pInfoList = pValueInfo->getMap();
				CLockMap<tstring, cgcValueInfo::pointer>::const_iterator pIter = pInfoList.begin();
				for (; pIter!=pInfoList.end(); pIter++)
				{
					if (bRoot && sKey.empty())	// 直接写到MAP里面，生成结果没有 {"":....}
						PutJson(ptResponse,pIter->first,pIter->second);
					else
						PutJson(ptChild,pIter->first,pIter->second);
				}
				if (bRoot && sKey.empty())
					break;
				if (sKey.empty())
					ptResponse.push_back(std::make_pair(L"", ptChild));
				else
					ptResponse.push_back(std::make_pair(conv::utf_to_utf<wchar_t>(sKey), ptChild));
			}break;
		case cgcValueInfo::TYPE_VECTOR:
			{
				wptree ptChild;
				const std::vector<cgcValueInfo::pointer>& pInfoList = pValueInfo->getVector();
				for (size_t i=0; i<pInfoList.size(); i++)
				{
					PutJson(ptChild,"",pInfoList[i]);
				}
				if (sKey.empty())
					ptResponse.push_back(std::make_pair(L"", ptChild));
				else
					ptResponse.push_back(std::make_pair(conv::utf_to_utf<wchar_t>(sKey), ptChild));
			}break;
		default:
			{
				if (sKey.empty())
					ptResponse.push_back(std::make_pair(L"", conv::utf_to_utf<wchar_t>(pValueInfo->toString())));
				else
					ptResponse.push_back(std::make_pair(conv::utf_to_utf<wchar_t>(sKey), conv::utf_to_utf<wchar_t>(pValueInfo->toString())));
				//if (bVector || (bRoot && sKey.empty()))
				//	ptResponse.push_back(std::make_pair(L"", conv::utf_to_utf<wchar_t>(pValueInfo->toString())));		// 数组
				//else
				//	ptResponse.put(conv::utf_to_utf<wchar_t>(sKey), conv::utf_to_utf<wchar_t>(pValueInfo->toString()));
			}break;
		}
	}
	bool toJson(const cgcValueInfo::pointer& inParam,cgcValueInfo::pointer outParam)
	{
		wptree ptResponse;
		PutJson(ptResponse,"",inParam,true);

		cgcValueInfo::pointer pOutParam = outParam.get() == NULL?inParam:outParam;
		pOutParam->totype(cgcValueInfo::TYPE_STRING);
		pOutParam->reset();
		try{

			std::wstringstream s2;
			write_json(s2, ptResponse,false);
			std::string sTextString(conv::utf_to_utf<char>(s2.str()));
			if ((inParam->getType()==cgcValueInfo::TYPE_VECTOR || inParam->getType()==cgcValueInfo::TYPE_VALUEINFO)
				&& sTextString.substr(0,4)=="{\"\":")
			{
				// 去掉开头的：{"":
				// 和去掉结尾的：}
				sTextString = sTextString.substr(4);
				const std::string::size_type find = sTextString.rfind("}");
				if (find != std::string::npos)
				{
					sTextString = sTextString.substr(0,find);
				}else
				{
					//  }后面还有一个空格，所有去掉二个
					sTextString = sTextString.substr(0,sTextString.size()-2);
				}
			}
			pOutParam->setStr(sTextString);
		}catch(ptree_error &) { 
			return false;
		}
		return true;
	}
	//std::string toHex(const std::string &src)
	//{
	//	std::string result;
	//	const char * srcBuffer = src.c_str();

	//	for (size_t i=0; i<src.size(); i++)
	//	{
	//		char srcChar = srcBuffer[i];
	//		unsigned char hexBuffer[3];
	//		if (srcChar > 0xF)
	//		{
	//			hexBuffer[2] = 0;
	//			hexBuffer[1] = toHex(srcChar & 0xF);
	//			hexBuffer[0] = toHex((srcChar>>4) & 0xF);
	//		}else
	//		{
	//			hexBuffer[1] = 0;
	//			hexBuffer[0] = toHex(srcChar & 0xF);
	//		}
	//		result.append((const char*)hexBuffer);
	//	}
	//	return result;
	//}

	unsigned char toHex(const unsigned char &x)
	{
		return x > 9 ? x-10 + 'A': x+'0';
	}

	unsigned char fromHex(const unsigned char &x)
	{
		return (x>=48&&x<=57) ? x-'0' : x-'A'+10;	// 48='0' 57='9'
		//return isdigit(x) ? x-'0' : x-'A'+10;
	}

	//void convert(char *input, char *output)
	//{
	//	int len1 = strlen(input); //输入二进制数位数
	//	int pos = len1 / 4 + 1; //输出十六进制数的位数
	//	if (len1 % 4 == 0)
	//	{
	//		pos = pos - 1;
	//	}
	//	int j = 0;
	//	while (len1>0)
	//	{
	//		char sum = 0;
	//		for (int i=0; i<4 && len1>0; i++, len1--) //从最后起每4位算一次值
	//		{
	//			sum = sum + (input[len1-1]-'0')*pow(2, i);
	//		}

	//		// 转换成16进制数表示
	//		sum = sum + '0';
	//		if ('9'<sum && sum<'9'+7)
	//		{
	//			sum = sum + 7;
	//		}
	//		else if (sum > '9' + 6)
	//		{
	//			printf("您输入的不是正确的2进制数!\n");
	//			exit(0);
	//		}

	//		//十六进制数放到output数组相应位置
	//		output[--pos] = sum;
	//	}
	//}

	// 16进制转2进制
	size_t convert_16t2(const unsigned char *sIn, size_t nEncLen,unsigned char *pOut)
	{
		size_t index = 0;
		for( size_t ix = 0; ix<(nEncLen-1); ix++ )
		{
			unsigned char ch = 0;
			ch = (fromHex(sIn[ix+0])<<4);
			ch |= fromHex(sIn[ix+1]);
			ix += 1;
			pOut[index++] = ch;
		}
		return index;
	}
	// 2进制转16进制
	std::string convert_2t16(const unsigned char *sIn, size_t nEncLen)
	{
		std::string sOut;
		for( size_t ix = 0; ix <nEncLen; ix++ )
		{
			unsigned char buf[3];
			memset( buf, 0, 3 );
			buf[0] = toHex( (unsigned char)sIn[ix] >> 4 );
			buf[1] = toHex( (unsigned char)sIn[ix] % 16);
			sOut += (char *)buf;
		}
		return sOut;
	}

	std::string URLEncode(const char *sIn)
	{
		std::string sOut;
		for( size_t ix = 0; ix < strlen(sIn); ix++ )
		{
			unsigned char buf[4];
			memset( buf, 0, 4 );
			if( isalnum( (unsigned char)sIn[ix] ) )
			{      
				buf[0] = sIn[ix];
			}
			else
			{
				buf[0] = '%';
				buf[1] = toHex( (unsigned char)sIn[ix] >> 4 );
				buf[2] = toHex( (unsigned char)sIn[ix] % 16);
			}
			sOut += (char *)buf;
		}
		return sOut;
	}

	std::string URLDecode(const char *sIn)
	{
		std::string sOut;
		for( size_t ix = 0; ix < strlen(sIn); ix++ )
		{
			unsigned char ch = 0;
			if(sIn[ix]=='%')
			{
				ch = (fromHex(sIn[ix+1])<<4);
				ch |= fromHex(sIn[ix+2]);
				ix += 2;
			}
			else if(sIn[ix] == '+')
			{
				ch = ' ';
			}
			else
			{
				ch = sIn[ix];
			}
			sOut += (char)ch;
		}

		return sOut;

	}



	//////////////////////////////////////////////////////////////
	virtual std::string W2Char(const wchar_t * strSource) const
	{
		std::string result = "";
		size_t targetLen = wcsrtombs(NULL, &strSource, 0, NULL);
		if (targetLen > 0)
		{
			char * pTargetData = new char[targetLen];
			memset(pTargetData, 0, (targetLen)*sizeof(char));
			wcsrtombs(pTargetData, &strSource, targetLen, NULL);
			result = pTargetData;
			delete[] pTargetData;
		}
		return result;
	}
	virtual std::wstring Char2W(const char * strSource) const
	{
		std::wstring result = L"";
		size_t targetLen = mbsrtowcs(NULL, (const char**)&strSource, 0, 0);
		if (targetLen > 0)
		{
			targetLen += 1;

			wchar_t * pTargetData = new wchar_t[targetLen];
			memset(pTargetData, 0, (targetLen)*sizeof(wchar_t));
			mbsrtowcs(pTargetData, &strSource, targetLen, 0);
			result = pTargetData;
			delete[] pTargetData;   
		}
		return result;
	}
	static void W2C(const wchar_t *pw, char *pc)
	{
		if (*pw < 255)
			*pc = *pw;
		else
		{
			*pc++ = *pw >> 8 ;
			*pc = *pw;
		}
	}
	//
	// char to wchar_t
	static void C2W(const char * pc, wchar_t * pw)
	{
		if (*pc >= 0)
			*pw = *pc;
		else
			*pw = (((pc[0] & 0xff) << 8) + (pc[1] & 0xff));
	}

	//
	// 把Unicode转换成UTF-8
	static void UnicodeToUTF_8(char* pOut, const wchar_t* pText)
	{
		// 注意 WCHAR高低字的顺序,低字节在前，高字节在后
		char* pchar = (char *)pText;
		pOut[0] = (0xE0 | ((pchar[1] & 0xF0) >> 4));
		pOut[1] = (0x80 | ((pchar[1] & 0x0F) << 2)) + ((pchar[0] & 0xC0) >> 6);
		pOut[2] = (0x80 | (pchar[0] & 0x3F));
	}

	// 把UTF-8转换成Unicode
	static void UTF_8ToUnicode(wchar_t* pOut, const char *pText)
	{
		char* uchar = (char *)pOut;
		uchar[1] = ((pText[0] & 0x0F) << 4) + ((pText[1] >> 2) & 0x0F);
		uchar[0] = ((pText[1] & 0x03) << 6) + (pText[2] & 0x3F);
	}
	//////////////////////////////////////////////////////////////
#ifdef WIN32
	virtual std::string convert(const char * strSource, int sourceCodepage, int targetCodepage) const
	{
		int unicodeLen = MultiByteToWideChar(sourceCodepage, 0, strSource, -1, NULL, 0);
		if (unicodeLen <= 0) return "";

		wchar_t * pUnicode = new wchar_t[unicodeLen];
		memset(pUnicode,0,(unicodeLen)*sizeof(wchar_t));

		MultiByteToWideChar(sourceCodepage, 0, strSource, -1, (wchar_t*)pUnicode, unicodeLen);

		char * pTargetData = 0;
		int targetLen = WideCharToMultiByte(targetCodepage, 0, (wchar_t*)pUnicode, -1, (char*)pTargetData, 0, NULL, NULL);
		if (targetLen <= 0)
		{
			delete[] pUnicode;
			return "";
		}

		pTargetData = new char[targetLen];
		memset(pTargetData, 0, targetLen);

		WideCharToMultiByte(targetCodepage, 0, (wchar_t*)pUnicode, -1, (char *)pTargetData, targetLen, NULL, NULL);

		std::string result = pTargetData;
		//	tstring result(pTargetData, targetLen);
		delete[] pUnicode;
		delete[] pTargetData;
		return   result;
	}
#endif

	//////////////////////////////////////////////////////////////
	virtual std::string format(const char * format,...) const
	{
		if (format == 0) return "";

		char formatmsg[MAX_STRING_FORMAT_SIZE+1];
		va_list   vl;
		va_start(vl, format);
		int len = vsnprintf(formatmsg, MAX_STRING_FORMAT_SIZE, format, vl);
		va_end(vl);
		if (len > MAX_STRING_FORMAT_SIZE)
			len = MAX_STRING_FORMAT_SIZE;

		formatmsg[len] = '\0';

		return std::string(formatmsg, len);
	}
	virtual const tstring & replace(tstring & strSource, const tstring & strFind, const tstring &strReplace) const
	{
		tstring::size_type pos=0;
		tstring::size_type findlen=strFind.size();
		tstring::size_type replacelen=strReplace.size();
		while ((pos=strSource.find(strFind, pos)) != tstring::npos)
		{
			strSource.replace(pos, findlen, strReplace);
			pos += replacelen;
		}
		return strSource;
	}

	virtual const tstring & toUpper(tstring & strSource) const
	{
		std::transform(strSource.begin(), strSource.end(), strSource.begin(), toupper);
		return strSource;
	}
	virtual const tstring & toLower(tstring & strSource) const
	{
		std::transform(strSource.begin(), strSource.end(), strSource.begin(), tolower);
		return strSource;
	}

	//////////////////////////////////////////////////////////////
	virtual int toInt(const char * strValue, int defaultValue) const
	{
		if (strValue == 0) return defaultValue;
		try
		{
			return atoi(strValue);
		}catch(std::exception const &)
		{
		}catch(...)
		{}
		return defaultValue;
	}
	virtual long toLong(const char * strValue, long defaultValue) const
	{
		if (strValue == 0) return defaultValue;
		try
		{
			return atol(strValue);
		}catch(std::exception const &)
		{
		}catch(...)
		{
		}
		return defaultValue;
	}
	virtual unsigned long toULong(const char * strValue, unsigned long defaultValue) const
	{
		if (strValue == 0) return defaultValue;
		try
		{
			char * stopstring;
			int base = 10;

			return strtoul(strValue, &stopstring, base);
		}catch(std::exception const &)
		{
		}catch(...)
		{
		}
		return defaultValue;
	}
	virtual double toDouble(const char * strValue, double defaultValue) const
	{
		if (strValue == 0) return defaultValue;
		try
		{
			return atof(strValue);
		}catch(std::exception const &)
		{
		}catch(...)
		{
		}
		return defaultValue;
	}
	int SplitString2List(const char * lpszString, const char * lpszInterval, std::vector<std::string> & pOut)
	{
		std::string sIn(lpszString);
		const size_t nIntervalLen = strlen(lpszInterval);
		pOut.clear();
		while (!sIn.empty())
		{
			std::string::size_type find = sIn.find(lpszInterval);
			if (find == std::string::npos)
			{
				pOut.push_back(sIn);
				break;
			}
			if (find==0)
				pOut.push_back("");	// 空
			else
				pOut.push_back(sIn.substr(0, find));
			sIn = sIn.substr(find+nIntervalLen);
		}
		return (int)pOut.size();
	}

};

CStringService::pointer gServicePointer;

extern "C" bool CGC_API CGC_Module_Init(void)
{
	return true;
}

extern "C" void CGC_API CGC_Module_Free(void)
{
	gServicePointer.reset();
}

extern "C" void CGC_API CGC_GetService(cgcServiceInterface::pointer & outService, const cgcValueInfo::pointer& parameter)
{
	if (gServicePointer.get() == NULL)
	{
		gServicePointer = CStringService::create();
	}
	outService = gServicePointer;
}
