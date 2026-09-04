#include "hsp3dish.as"

#regcmd "hsp3cmdinit","hpi3sample.so"
#cmd newcmd $000
#cmd newcmd2 $001
#cmd newcmd3 $002
; #cmd newcmd4 $003

*main
redraw 0
a=0

pos 0, 0
color 255, 255, 255: boxf
color 0, 0, 0

newcmd 1000		; 省略時は123となります
mes "システム変数statの値は、"+stat+"です。"

a=12
mes "関数newcmd("+a+")の値="+newcmd(a)+"です。"

repeat 12
newcmd2 a,10		; 0～9までの乱数を変数aに代入
mes "乱数="+a
loop

newcmd3 "test.txt",10.0
mes "test.txtを作成しました。"

; newcmd4 300,50,500,250,0	; 線を描画
; newcmd4 300,50,100,250,0	; 線を描画
; newcmd4 100,250,500,250,0	; 線を描画

redraw 1
await 100
goto *main
