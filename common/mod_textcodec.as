;
; ICU-based text encoding conversion module.
;
; Buffer arguments are passed by reference. Use textcodec_get_result after
; each command to obtain the converted byte count or -1 on error. The only
; implemented options are textconv_bom_add and textconv_bom_remove; ICU's
; default substitution policy is always used.
#ifndef __MOD_TEXTCODEC
#define __MOD_TEXTCODEC

; Windows ships an unversioned ICU forwarding DLL. Linux ICU 70 exports
; symbols with the _70 suffix.
#ifdef _hspwin
#uselib "icuuc.dll"
#cfunc global textcodec_ucnv_open "ucnv_open" sptr,var
#func global textcodec_ucnv_close "ucnv_close" sptr
#cfunc global textcodec_ucnv_to_u "ucnv_toUChars" sptr,var,int,var,int,var
#cfunc global textcodec_ucnv_from_u "ucnv_fromUChars" sptr,var,int,var,int,var
#else
#uselib "libicuuc.so.70"
#cfunc global textcodec_ucnv_open "ucnv_open_70" sptr,var
#func global textcodec_ucnv_close "ucnv_close_70" sptr
#cfunc global textcodec_ucnv_to_u "ucnv_toUChars_70" sptr,var,int,var,int,var
#cfunc global textcodec_ucnv_from_u "ucnv_fromUChars_70" sptr,var,int,var,int,var
#endif

#define global textconv_src_error (0)
#define global textconv_src_substitute (1)
#define global textconv_src_skip (2)
#define global textconv_dst_error (0)
#define global textconv_dst_substitute (4)
#define global textconv_dst_skip (8)
#define global textconv_bom_remove ($100)
#define global textconv_bom_add ($200)
#define global textcodec_buffer_overflow (15)

_textcodec_result = 0
#module "textcodec"

#defcfunc local textcodec_internal_encoding

    ; HSPランタイムが使用する内部文字コード名を取得する
    ; 戻り値 : UTF-8ランタイムでは"UTF-8"、それ以外では"CP932"
    ;
    if hspstat & $20000 : return "UTF-8"
    return "CP932"

#defcfunc local textcodec_has_bom str _encoding

    ; 指定した文字コードがBOMを使用できるかを調べる
    ; textcodec_has_bom 文字コード名
    ; 戻り値 : BOMを使用できる場合は1、それ以外は0
    ;
    if _encoding = "UTF-8" : return 1
    if _encoding = "UTF-16" : return 1
    if _encoding = "UTF-16LE" : return 1
    if _encoding = "UTF-16BE" : return 1
    if _encoding = "UTF-32" : return 1
    if _encoding = "UTF-32LE" : return 1
    if _encoding = "UTF-32BE" : return 1
    return 0

#deffunc text_cnvtext var _destination, var _source, str _srcencoding, str _dstencoding, int _flags, int _source_size

    ; 文字コードを変換する
    ; text_cnvtext 出力変数, 入力変数, 入力文字コード名, 出力文字コード名, オプション, 入力バイト数
    ; 結果はtextcodec_get_resultで取得する
    ;
    if _source_size < 0 {
        _textcodec_result@ = -1
        return
    }
    textcodec_status = 0
    textcodec_src = textcodec_ucnv_open(_srcencoding, textcodec_status)
    if textcodec_status > 0 | textcodec_src = 0 {
        _textcodec_result@ = -1
        return
    }

    sdim textcodec_dummy, 1
    textcodec_status = 0
    textcodec_wide_size = textcodec_ucnv_to_u(textcodec_src, textcodec_dummy, 0, _source, _source_size, textcodec_status)
    if textcodec_status != 0 & textcodec_status != textcodec_buffer_overflow {
        textcodec_ucnv_close textcodec_src
        _textcodec_result@ = -1
        return
    }
    sdim textcodec_wide, (textcodec_wide_size + 1) * 2
    textcodec_status = 0
    textcodec_result = textcodec_ucnv_to_u(textcodec_src, textcodec_wide, textcodec_wide_size + 1, _source, _source_size, textcodec_status)
    textcodec_ucnv_close textcodec_src
    if textcodec_status > 0 {
        _textcodec_result@ = -1
        return
    }

    if (_flags & textconv_bom_remove) & textcodec_wide_size > 0 {
        if wpeek(textcodec_wide, 0) = $feff {
            if textcodec_wide_size > 1 {
                repeat (textcodec_wide_size - 1) * 2
                    poke textcodec_wide, cnt, peek(textcodec_wide, cnt + 2)
                loop
            }
            wpoke textcodec_wide, (textcodec_wide_size - 1) * 2, 0
            textcodec_wide_size--
        }
    }

    textcodec_bom_used = 0
    if (_flags & textconv_bom_add) {
        if textcodec_has_bom@textcodec(_dstencoding) {
            if textcodec_wide_size = 0 | wpeek(textcodec_wide, 0) ! $feff {
                sdim textcodec_wide_bom, (textcodec_wide_size + 2) * 2
                wpoke textcodec_wide_bom, 0, $feff
                textcodec_copy_size = textcodec_wide_size * 2
                if textcodec_wide_size > 0 {
                    repeat textcodec_copy_size
                        poke textcodec_wide_bom, cnt + 2, peek(textcodec_wide, cnt)
                    loop
                }
                wpoke textcodec_wide_bom, (textcodec_wide_size + 1) * 2, 0
                textcodec_bom_used = 1
                textcodec_wide_size++
            }
        }
    }

    textcodec_status = 0
    textcodec_dst = textcodec_ucnv_open(_dstencoding, textcodec_status)
    if textcodec_status > 0 | textcodec_dst = 0 {
        _textcodec_result@ = -1
        return
    }

    sdim textcodec_dummy, 1
    textcodec_status = 0
    if textcodec_bom_used {
        textcodec_size = textcodec_ucnv_from_u(textcodec_dst, textcodec_dummy, 0, textcodec_wide_bom, textcodec_wide_size, textcodec_status)
    } else {
        textcodec_size = textcodec_ucnv_from_u(textcodec_dst, textcodec_dummy, 0, textcodec_wide, textcodec_wide_size, textcodec_status)
    }
    if textcodec_status != 0 & textcodec_status != textcodec_buffer_overflow {
        textcodec_ucnv_close textcodec_dst
        _textcodec_result@ = -1
        return
    }
    sdim textcodec_encoded, textcodec_size + 1
    textcodec_status = 0
    if textcodec_bom_used {
        textcodec_result = textcodec_ucnv_from_u(textcodec_dst, textcodec_encoded, textcodec_size + 1, textcodec_wide_bom, textcodec_wide_size, textcodec_status)
    } else {
        textcodec_result = textcodec_ucnv_from_u(textcodec_dst, textcodec_encoded, textcodec_size + 1, textcodec_wide, textcodec_wide_size, textcodec_status)
    }
    textcodec_ucnv_close textcodec_dst
    if textcodec_status > 0 {
        _textcodec_result@ = -1
        return
    }

    sdim _destination, textcodec_size + 1
    if textcodec_size > 0 {
        repeat textcodec_size
            poke _destination, cnt, peek(textcodec_encoded, cnt)
        loop
    }
    _textcodec_result@ = textcodec_size
    return

#deffunc text_loadfile var _destination, str _filename, int _limit, str _encoding, int _flags

    ; テキストファイルを読み込み、HSPの内部文字コードに変換する
    ; text_loadfile 出力変数, ファイル名, 最大読込バイト数, 入力文字コード名, オプション
    ; 最大読込バイト数を-1にするとファイル全体を読み込む
    ; 文字コード名を空文字列にすると変換せずに読み込む
    ; 結果はtextcodec_get_resultで取得する
    ;
    exist _filename
    textcodec_size = strsize
    if textcodec_size < 0 {
        _textcodec_result@ = -1
        return
    }
    if _limit >= 0 & textcodec_size > _limit : textcodec_size = _limit

    sdim textcodec_raw, textcodec_size + 1
    bload _filename, textcodec_raw, textcodec_size
    textcodec_size = strsize
    if _encoding = "" {
        sdim _destination, textcodec_size + 1
        if textcodec_size > 0 {
            repeat textcodec_size
                poke _destination, cnt, peek(textcodec_raw, cnt)
            loop
        }
        _textcodec_result@ = textcodec_size
        return
    }
    text_cnvtext _destination, textcodec_raw, _encoding, textcodec_internal_encoding@textcodec(), _flags, textcodec_size
    return

#deffunc text_savefile var _source, str _filename, str _encoding, int _flags, int _source_size

    ; HSPの内部文字コードから変換してテキストファイルに保存する
    ; text_savefile 入力変数, ファイル名, 出力文字コード名, オプション, 入力バイト数
    ; 入力バイト数を省略するか0以下にするとstrlen(入力変数)を使用する
    ; 埋め込みNULを含む入力では正しい入力バイト数を指定する
    ; 文字コード名を空文字列にすると変換せずに保存する
    ; 結果はtextcodec_get_resultで取得する
    ;
    textcodec_size = _source_size
    if textcodec_size <= 0 : textcodec_size = strlen(_source)
    if _encoding = "" {
        bsave _filename, _source, textcodec_size
        _textcodec_result@ = textcodec_size
        return
    }
    sdim textcodec_output, 1
    text_cnvtext textcodec_output, _source, textcodec_internal_encoding@textcodec(), _encoding, _flags, textcodec_size
    textcodec_size = _textcodec_result@
    if textcodec_size < 0 : return
    bsave _filename, textcodec_output, textcodec_size
    _textcodec_result@ = textcodec_size
    return

#deffunc textcodec_get_result var _result

    ; 直前に実行した変換、読込、保存の結果を取得する
    ; textcodec_get_result 結果変数
    ; 結果 : 成功時は処理したバイト数、失敗時は-1
    ;
    _result = int(_textcodec_result@)
    return

#global
#endif
