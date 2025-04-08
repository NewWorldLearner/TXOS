
BaseOfStack    equ 0x7c00
BaseOfLoader   equ 0x1000
OffsetOfLoader equ 0x00

LOADER_START_SECTOR    equ 2

SECTION MBR vstart=0x7c00

;BIOS跳转到mbr时，执行的指令为jmp 0x00:0x7c00,因此cs的值为0x00
Label_Start:
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, BaseOfStack

;======= clear screen
;int 10h,AH=06功能，按指定范围滚动窗口
;AL=滚动的列数,为0时清空屏幕
;BH=滚动后空出位置放入的属性
;CH=滚动范围的左上角坐标列号
;CL=滚动范围的左上角坐标行号
;DH=滚动范围的右下角坐标列号
;DL=滚动范围的右下角坐标行号
;BH=颜色属性
    mov ax, 0600h   ;调用06H号功能清空屏幕
    mov bx, 0700h   ;滚动后放入的属性，黑底白字
    mov cx, 0
    mov dx, 0184fh
    int 10h

;======= set focus
;int 10H,AH=02号功能
;DH=游标的列数
;DL=游标的行数
;BH=页码
    mov ax, 0200h
    mov bx, 0000h
    mov dx, 0000h
    int 10h

;======= display boot message
;int 10H,AH=13h功能，实现字符串的显示
;AL=写入模式，AL=01时，字符串的属性由BL提供，CX寄存器提供字符串长度，显示后光标移到到字符串显示位置的末尾
;ES:BP=要显示的字符串的内存地址
;DH=游标的行号
;DL=游标的列好
;BH=页码
    mov ax, 1301h   ;0x10号中断的13号子功能，显示模式为01
    mov bx, 000fh   ;黑底高亮白字
    mov dx, 0000h   ;显示位置，0行0列
    mov cx, 13      ;显示13个字符
    push ax
    mov ax, ds
    mov es, ax
    pop ax
    mov bp, StartBootMessage
    int 10h

;加载loader程序
   mov ax,BaseOfLoader
   mov ds,ax

   mov eax,LOADER_START_SECTOR	 ; 起始扇区lba地址
   mov bx,OffsetOfLoader         ; 写入的地址 
   mov cx,10			 		 ; 待读入的扇区数
   call rd_disk_m_16		     ;以下读取程序的起始部分（一个扇区）

;跳转到loader程序去执行
	jmp BaseOfLoader:OffsetOfLoader

;-------------------------------------------------------------------------------
;功能:读取硬盘n个扇区
rd_disk_m_16:	   
;-------------------------------------------------------------------------------
				       ; eax=LBA扇区号
				       ; ebx=将数据写入的内存地址
				       ; ecx=读入的扇区数
      mov esi,eax	  ;备份eax
      mov di,cx		  ;备份cx
;读写硬盘:
;第1步：设置要读取的扇区数
      mov dx,0x1f2
      mov al,cl
      out dx,al            ;读取的扇区数

      mov eax,esi	   ;恢复ax

;第2步：将LBA地址存入0x1f3 ~ 0x1f6

      ;LBA地址7~0位写入端口0x1f3
      mov dx,0x1f3                       
      out dx,al                          

      ;LBA地址15~8位写入端口0x1f4
      mov cl,8
      shr eax,cl
      mov dx,0x1f4
      out dx,al

      ;LBA地址23~16位写入端口0x1f5
      shr eax,cl
      mov dx,0x1f5
      out dx,al

      shr eax,cl
      and al,0x0f	   ;lba第24~27位
      or al,0xe0	   ; 设置7～4位为1110,表示lba模式
      mov dx,0x1f6
      out dx,al

;第3步：向0x1f7端口写入读命令，0x20 
      mov dx,0x1f7
      mov al,0x20                        
      out dx,al

;第4步：检测硬盘状态
  .not_ready:
      ;同一端口，写时表示写入命令字，读时表示读入硬盘状态
      nop
      in al,dx
      and al,0x88	   ;第4位为1表示硬盘控制器已准备好数据传输，第7位为1表示硬盘忙
      cmp al,0x08
      jnz .not_ready	   ;若未准备好，继续等。

;第5步：从0x1f0端口读数据
      mov ax, di
      mov dx, 256
      mul dx
      mov cx, ax	   ; di为要读取的扇区数，一个扇区有512字节，每次读入一个字，
			   ; 共需di*512/2次，所以di*256
      mov dx, 0x1f0
  .go_on_read:
      in ax,dx
      mov [bx],ax
      add bx,2		  
      loop .go_on_read
      ret

;======= messages
StartBootMessage: db "Start Booting"

;======= fill zero
times 510-($-$$) db 0
dw 0xaa55