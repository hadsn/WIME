#pragma once

#define AT_OK		0
#define AT_FAIL		-1
#define AT_NOTATOK	-2
#define ATASSISTDICMAX			4
#define ATDICFILENAME_MAX		(256+2)
#define ATDICFILESETNICKNAME_MAX	(80+1)
#define	ATCHECKVERSION			0
#define	ATCHECKVERSION_ORGREATER	1

//表示色
#define ATCOLINDEX_INPUT			0x00	//変換可能入力文字
#define ATCOLINDEX_CONVERTED			0x01	//変換文字
#define ATCOLINDEX_TARGETCONVERT		0x02    //変換済み注目文節
#define ATCOLINDEX_TARGETNOTCONVERTED		0x03	//未変換注目文節
#define ATCOLINDEX_INPUT_ERROR			0x04	//入力文字エラー
#define ATCOLINDEX_INPUTKOTEI			0x06	//固定入力文字
#define ATCOLINDEX_TARGETNOTCONVERTEDKOTEI	0x08	//固定入力文節
#define ATCOLINDEX_TARGETCOMMENT		0x09	//注目文節コメント
#define ATCOLINDEX_COMMENT			0x0a	//文節コメント

// 表示色アトリビュート構造体
typedef struct{
    uint32_t	Back;		//背景色:COLORREF
    uint32_t	Text;		//文字色
    int		UnderLine;	//下線表示有無:BOOL
}__attribute__((packed)) ATImeCol;
#define	ATIMECOMPCOL_ITEMMAX	16	//未確定文字表示項目数

#define GETR(rgb) ((rgb)&0xff)
#define GETG(rgb) (((rgb)>>8)&0xff)
#define GETB(rgb) (((rgb)>>16)&0xff)
#define COL8TO16(i8) ((i8)*0xffff/0xff)
#define GETR16(rgb) COL8TO16(GETR(rgb))
#define GETG16(rgb) COL8TO16(GETG(rgb))
#define GETB16(rgb) COL8TO16(GETB(rgb))

//(C) 2020 thomas
