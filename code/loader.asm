KERNEL_START_SECTOR equ 12							;内核在磁盘中的起始扇区
KERNEL_BIN_BASE_ADDR equ	0x100000				;内核在内存在的起始地址

MemoryStructBufferAddr	equ	0x7E00					;物理内存布局信息的存放处

SECTION LOADER vstart=0x10000
	jmp Start_Loader

;------------------要进入64位模式，首先要进入32位保护模式，定义32位模式下的GDT表--------------

[SECTION gdt32]

GDT32_BASE:		dd	0,0								;第0个描述符不能使用
GDT32_DESC_CODE:	dd	0x0000FFFF,0x00CF9A00		;代码段描述符
GDT32_DESC_DATA:	dd	0x0000FFFF,0x00CF9200		;栈段描述符

GDT32_SIZE   equ   $ - GDT32_BASE					;GDT的内存地址
GDT32Ptr	dw	GDT32_SIZE - 1						;描述GDT表的位置和大小
			dd	GDT32_BASE

SelectorCode32	equ	GDT32_DESC_CODE - GDT32_BASE
SelectorData32	equ	GDT32_DESC_DATA - GDT32_BASE

;------------------64位模式下的GDT表，忽略段起始地址属性，默认为0，忽略段限长属性---------------
;G位粒度位被忽略，因为段限长这个属性没有意义，因此粒度位也没有意义
;L位被置1，如果复位的话，会回到32位模式
;在64位模式下，D位为0时代码段操作数位宽为32，D位为1时触发#GP异常？？？？有待验证
;S位为1时表示代码段或系统段

[SECTION gdt64]

GDT64_BASE:		dq	0x0000000000000000
GDT64_DESC_CODE:	dq	0x0020980000000000				;低32位是段界限和段基址的一部分，在64位下全为0,高4字节的低8位也是段界限，因此为0
														;TYPE=1000，只执行的代码段
														;P=1,DPL=0,S=1
														;L=1

GDT64_DESC_DATA:	dq	0x0000920000000000				;TYPE=0010,向上扩展，可读写的数据段
														;P=1,DPL=0,S=1

GDT64_SIZE	equ	$ - GDT64_BASE
GDTPtr64	dw	GDT64_SIZE - 1
			dd	GDT64_BASE

SelectorCode64	equ	GDT64_DESC_CODE - GDT64_BASE
SelectorData64	equ	GDT64_DESC_DATA - GDT64_BASE


[SECTION .s16]
[BITS 16]

Start_Loader:
	mov	ax,	cs
	mov	ds,	ax
	mov	es,	ax
	mov	ax,	0x00
	mov	ss,	ax
	mov	sp,	0x7c00
	mov ax,0xb800
	mov gs,ax


;----------------display on screen : Start Loader-------------------

	mov	ax,	1301h
	mov	bx,	000fh
	mov	dx,	0100h		;row 1
	mov	cx,	12
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	StartLoaderMessage
	int	10h


;--------------进入保护模式让FS的寻址能力超过1MB，然后再退出保护模式，但如果直接使用ebx呢？---------------------
	push	ax						;稍后测试去掉这条语句，应该没影响
	in	al,	92h						;A20地址线的控制开关是0x92端口的位1
	or	al,	00000010b
	out	92h,	al
	pop	ax							;稍后测试去掉这条语句，应该没影响

	cli								;我们还未建立保护模式下的中断描述符表，因此进入保护模式时先关中断
	;db 0x66						;现代编译器不推荐在lgdt指令前手动加上前缀
	lgdt	[GDT32Ptr]				;加载32位GDT

	mov	eax,	cr0					;置位CR0的第0位
	or	eax,	1
	mov	cr0,	eax

	mov	ax,	SelectorData32
	mov	fs,	ax
	mov	eax,	cr0
	and	al,	11111110b
	mov	cr0,	eax

	sti

;-----------------------------加载内核到1MB内存处----------------------

   	mov	ax,	1301h
	mov	bx,	000Fh
	mov	dx,	0200h		;row 2
	mov	cx,	17
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	STARTLOADKERNEL
	int	10h

;   mov eax, KERNEL_START_SECTOR        ; kernel.bin所在的扇区号
;   mov ebx, 0x100000       ; 从磁盘读出后，写入到ebx指定的地址
;   mov ecx, 10			       		   ; 读入的扇区数，先假设内核大小在150KB以内
;   call rd_disk_m_32				   ;借助ebx而不是使用fs会有问题吗？稍后需要测试一下


   	mov	ax,	1301h
	mov	bx,	000Fh
	mov	dx,	0300h		;row 3
	mov	cx,	22
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	ENDLoadKERNEL
	int	10h

;------------------------获取物理内存信息---------------
	mov	ax,	1301h
	mov	bx,	000Fh
	mov	dx,	0400h		;row 4
	mov	cx,	24
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	StartGetMemStructMessage
	int	10h

	mov	ebx,	0						;第一次调用需要为0
	mov	ax,	0x00
	mov	es,	ax
	mov	di,	MemoryStructBufferAddr		;地址范围描述符结构体ARDS的存放地址

Label_Get_Mem_Struct:

	mov	eax,	0x0E820					;int15,eax=0xe820功能号，用于获取物理内存布局信息，每次返回一个内存段信息
	mov	ecx,	20						;地址范围描述符结构体ARDS的大小
	mov	edx,	0x534D4150				;固定签名，它是SMAP的ASCII码值
	int	15h
	jc	Label_Get_Mem_Fail				;调用失败时，CF标志位为1
	add	di,	20

	cmp	ebx,	0						;调用结束后，ebx是下一个ARDS的地址，若为0，则表示本次返回的是最后一个ARDS结构体
	jne	Label_Get_Mem_Struct
	jmp	Label_Get_Mem_OK

Label_Get_Mem_Fail:

	mov	ax,	1301h
	mov	bx,	008Ch
	mov	dx,	0500h		;row 5
	mov	cx,	23
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	GetMemStructErrMessage
	int	10h
	jmp	$

Label_Get_Mem_OK:
	
	mov	ax,	1301h
	mov	bx,	000Fh
	mov	dx,	0500h		;row 5
	mov	cx,	29
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	GetMemStructOKMessage
	int	10h	


;------------------------------获取VBE控制信息-----------------------

	mov	ax,	1301h
	mov	bx,	000Fh
	mov	dx,	0600h		;row 6
	mov	cx,	23
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	StartGetSVGAVBEInfoMessage
	int	10h

	mov	ax,	0x00
	mov	es,	ax
	mov	di,	0x8000				;VBEInfoBloack信息块结构的保存地址
	mov	ax,	4F00h				;AH=4F是固定的,AL=00H,VBE的功能号，获取VBE控制信息

	int	10h						;调用成功AH=00h，否则AH保存失败类型，AL=AFH，支持该功能，否则不支持

	cmp	ax,	004Fh

	jz	.KO

;=======	Fail

	mov	ax,	1301h
	mov	bx,	008Ch
	mov	dx,	0700h		;row 7
	mov	cx,	23
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	GetSVGAVBEInfoErrMessage
	int	10h

	jmp	$

.KO:

	mov	ax,	1301h
	mov	bx,	000Fh
	mov	dx,	0700h		;row 7
	mov	cx,	29
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	GetSVGAVBEInfoOKMessage
	int	10h

;----------------------------获取VBE模式信息-------------

	mov	ax,	1301h
	mov	bx,	000Fh
	mov	dx,	0800h		;row 8
	mov	cx,	24
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	StartGetSVGAModeInfoMessage
	int	10h


	mov	ax,	0x00
	mov	es,	ax
	mov	si,	0x800e										;注意到VBE控制信息是保存在0x8000这个地方的，偏移量为14的地方保存的是VideoModeList指针

	mov	esi,	dword	[es:si]							;将VDE模式信息的内存地址付给esi
	mov	edi,	0x8200									;每个模式号都有1个模式信息块结构，它们保存在0x8200处
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	jmp Label_SVGA_Mode_Info_Finish

Label_SVGA_Mode_Info_Get:								;开始遍历模式列表

	mov	cx,	word	[es:esi]							;VBE模式号占用0-13位，D14=1采用线性帧缓冲区，D15=1保留显示缓存，每个模式号占两字节
														;由于VBE中存在多个模式号，因此VideoModeList指针指向的是模式号的数组

;-------------------------------VBE的模式信息占两字节，用16进制显示出这两个字节的值--------------------------

	push	ax
	
	mov	ax,	00h
	mov	al,	ch
	call	Label_DispAL								;显示VBE模式的高字节

	mov	ax,	00h
	mov	al,	cl	
	call	Label_DispAL							    ;显示VBE模式的低字节
	
	pop	ax

;=======
	
	cmp	cx,	0FFFFh										;模式数组中最后一个字的值为0xFFFF，表示模式数组的结束
	jz	Label_SVGA_Mode_Info_Finish

	mov	ax,	4F01h										;Al=01Ah,获取指定模式号的VBE模式信息
	int	10h

	cmp	ax,	004Fh										;返回值AL!=4FH则说明调用失败

	jnz	Label_SVGA_Mode_Info_FAIL						;这里很奇怪，在多次遍历的时候，某一次执行到该语句使得虚拟机卡死了

	add	esi,	2										;指向模式数组中的下一个模式号
	add	edi,	0x100									;下一个模式信息的保存地址

	jmp	Label_SVGA_Mode_Info_Get
		
Label_SVGA_Mode_Info_FAIL:

	mov	ax,	1301h
	mov	bx,	008Ch
	mov	dx,	0900h		;row 9
	mov	cx,	24
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	GetSVGAModeInfoErrMessage
	int	10h

Label_SET_SVGA_Mode_VESA_VBE_FAIL:

	jmp	$

Label_SVGA_Mode_Info_Finish:

	mov	ax,	1301h
	mov	bx,	000Fh
	mov	dx,	0E00h		;row 14
	mov	cx,	30
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	GetSVGAModeInfoOKMessage
	int	10h

;--------------------------设置VBE显示模式-----------------
;VBE模式设置成功以后，我们不能再通过int 10中断来显示字符串了，原因是VBE图形模式会禁用BIOS文本模式缓冲区(0xB8000)

	mov	ax,	4F02h							;AL=0x02,设置VBE显示模式
	mov	bx,	4183h							;bx=VBE模式号，mode : 0x180 or 0x143，设置为0x180时失败了
	int 	10h

	cmp	ax,	004Fh							;返回值AL!=4FH则说明调用失败
	jnz	Label_SET_SVGA_Mode_VESA_VBE_FAIL

;-----------------------进入保护模式----------------------

	cli

	db	0x66
	lgdt	[GDT32Ptr]

;	db	0x66
;	lidt	[IDT_POINTER]

	mov	eax,	cr0
	or	eax,	1
	mov	cr0,	eax

	jmp	dword SelectorCode32:GO_TO_TMP_Protect


[SECTION .s32]
[BITS 32]

GO_TO_TMP_Protect:
	mov	ax,	0x10
	mov	ds,	ax
	mov	es,	ax
	mov	fs,	ax
	mov	ss,	ax
	mov	esp,0x7c00

	call	support_long_mode
	test	eax,	eax

	jz	no_support

;=======	init temporary page table 0x90000
;-----------页顶级目录设置在0x90000这个内存地址-----------
	mov	dword	[0x90000],	0x91007				;P=1,RW=1,U=1
	mov	dword	[0x90800],	0x91007		

;-----------页顶级目录占据4KB，因此接下来是页上级目录----------
	mov	dword	[0x91000],	0x92007

;-----------页中级目录的位置是0x92000，由于使用的是2MB的页，因此省略掉了页表（一个页表512项*4KB页刚好是2MB）--------
;-----------处理器是如何知道我们省略掉了页表呢？是因为位7被设置为1吗？-------------
	mov	dword	[0x92000],	0x000083		;位7的值为1时，表示启用大页，一个页占2MB（或1GB），该位在64位模式下才有用

	mov	dword	[0x92008],	0x200083

	mov	dword	[0x92010],	0x400083

	mov	dword	[0x92018],	0x600083

	mov	dword	[0x92020],	0x800083

	mov	dword	[0x92028],	0xa00083

;=======	load GDTR

	lgdt	[GDTPtr64]
	mov	ax,	0x10
	mov	ds,	ax
	mov	es,	ax
	mov	fs,	ax
	mov	gs,	ax
	mov	ss,	ax

	mov	esp,	7c00h

;=======	open PAE

	mov	eax,	cr4				;CR4寄存器的第5位是PAE功能的标志位
	bts	eax,	5				;将目标操作数的指定位复制到CF标志位，并将该位置1
	mov	cr4,	eax

;=======	load	cr3

	mov	eax,	0x90000
	mov	cr3,	eax

;=======	enable long-mode

	mov	ecx,	0C0000080h		;IA32_EFER寄存器编号
	rdmsr						;读取EFER到EDX:EAX

	bts	eax,	8				;设置第8位（LME=1，启用长模式）
	wrmsr						;写回EFER

;=======	open PE and paging

	mov	eax,	cr0
	bts	eax,	0				;设置第0位（PE=1，启用保护模式）
	bts	eax,	31				;设置第31位（PG=1，启用分页）
	mov	cr0,	eax

	jmp	SelectorCode64:KERNEL_BIN_BASE_ADDR

;-------------------------------------------------------------------------------
;功能:检测是否支持长模式
;输入：无
;输出：EAX=1时表示支持长模式，EAX=0时表示不支持长模式
;-------------------------------------------------------------------------------

support_long_mode:

	mov	eax,	0x80000000				;当eax=0x80000000时，cpuid返回CPU支持的最高扩展功能号
	cpuid
	cmp	eax,	0x80000001
	setnb	al							;上面的结果不小于时，al被设置为1，否则al被设置为0
	jb	support_long_mode_done			;jb指令仅依赖CF标志位，也就是上面的eax小于0x80000001时返回0值，否则就继续检测
	mov	eax,	0x80000001				;当eax=0x80000001时，返回扩展功能标志（edx 的第29位表示长模式支持）
	cpuid
	bt	edx,	29						;bt 目标操作数, 位序号  ; 将目标操作数的指定位的值复制到 CF（进位标志）中,相比test指令更高效方便
	setc	al
support_long_mode_done:
	
	movzx	eax,	al
	ret

;=======	no support

no_support:
	jmp	$


;-------------------------------------------------------------------------------
;功能:读取硬盘n个扇区
rd_disk_m_32:	   
;-------------------------------------------------------------------------------
							 ; eax=LBA扇区号
							 ; ebx=将数据写入的内存地址
							 ; ecx=读入的扇区数
      mov esi,eax	   ; 备份eax
      mov di,cx		   ; 备份扇区数到di
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

;;;;;;; 至此,硬盘控制器便从指定的lba地址(eax)处,读出连续的cx个扇区,下面检查硬盘状态,不忙就能把这cx个扇区的数据读出来

;第4步：检测硬盘状态
  .not_ready:		   ;测试0x1f7端口(status寄存器)的的BSY位
      ;同一端口,写时表示写入命令字,读时表示读入硬盘状态
      nop
      in al,dx
      and al,0x88	   ;第4位为1表示硬盘控制器已准备好数据传输,第7位为1表示硬盘忙
      cmp al,0x08
      jnz .not_ready	   ;若未准备好,继续等。

;第5步：从0x1f0端口读数据
      mov ax, di	   ;以下从硬盘端口读数据用insw指令更快捷,不过尽可能多的演示命令使用,
			   ;在此先用这种方法,在后面内容会用到insw和outsw等

      mov dx, 256	   ;di为要读取的扇区数,一个扇区有512字节,每次读入一个字,共需di*512/2次,所以di*256
      mul dx
      mov cx, ax	   
      mov dx, 0x1f0
  .go_on_read:
      in ax,dx		
      mov [fs:ebx], ax
      add ebx, 2
			  ; 由于在实模式下偏移地址为16位,所以用bx只会访问到0~FFFFh的偏移。
			  ; loader的栈指针为0x900,bx为指向的数据输出缓冲区,且为16位，
			  ; 超过0xffff后,bx部分会从0开始,所以当要读取的扇区数过大,待写入的地址超过bx的范围时，
			  ; 从硬盘上读出的数据会把0x0000~0xffff的覆盖，
			  ; 造成栈被破坏,所以ret返回时,返回地址被破坏了,已经不是之前正确的地址,
			  ; 故程序出会错,不知道会跑到哪里去。
			  ; 所以改为ebx代替bx指向缓冲区,这样生成的机器码前面会有0x66和0x67来反转。
			  ; 0X66用于反转默认的操作数大小! 0X67用于反转默认的寻址方式.
			  ; cpu处于16位模式时,会理所当然的认为操作数和寻址都是16位,处于32位模式时,
			  ; 也会认为要执行的指令是32位.
			  ; 当我们在其中任意模式下用了另外模式的寻址方式或操作数大小(姑且认为16位模式用16位字节操作数，
			  ; 32位模式下用32字节的操作数)时,编译器会在指令前帮我们加上0x66或0x67，
			  ; 临时改变当前cpu模式到另外的模式下.
			  ; 假设当前运行在16位模式,遇到0X66时,操作数大小变为32位.
			  ; 假设当前运行在32位模式,遇到0X66时,操作数大小变为16位.
			  ; 假设当前运行在16位模式,遇到0X67时,寻址方式变为32位寻址
			  ; 假设当前运行在32位模式,遇到0X67时,寻址方式变为16位寻址.

      loop .go_on_read
      ret

;-------------------------------------------------------------------------------
;功能:将数字转换成16进制显示
;输入：AL=要显示的16进制数
;输出：DisplayPosition=转换后的16进制的内存地址
;-------------------------------------------------------------------------------

Label_DispAL:

	push	ecx
	push	edx
	push	edi
	
	mov	edi,	[DisplayPosition]
	mov	ah,	0Fh
	mov	dl,	al
	shr	al,	4						;首先获取al的高4位
	mov	ecx,	2
.begin:

	and	al,	0Fh
	cmp	al,	9
	ja	.1
	add	al,	'0'
	jmp	.2
.1:

	sub	al,	0Ah
	add	al,	'A'						;转换为16进制字符
.2:

	mov	[gs:edi],	ax				;将16进制字符存放在edi指向的内存处,这里gs的值为0xb800
	add	edi,	2
	
	mov	al,	dl
	loop	.begin

	mov	[DisplayPosition],	edi		;将转换后的16进制字符存放到这个位置

	pop	edi
	pop	edx
	pop	ecx
	
	ret

;---------IDT

IDT:
	times	0x50	dq	0
IDT_END:

IDT_POINTER:
		dw	IDT_END - IDT - 1
		dd	IDT

MemStructNumber		dd	0

SVGAModeCounter		dd	0

DisplayPosition		dd	0

;=======	display messages

StartLoaderMessage:	db	"Start Loader"
NoLoaderMessage:	db	"ERROR:No KERNEL Found"
KernelFileName:		db	"KERNEL  BIN",0
StartGetMemStructMessage:	db	"Start Get Memory Struct."
GetMemStructErrMessage:	db	"Get Memory Struct ERROR"
GetMemStructOKMessage:	db	"Get Memory Struct SUCCESSFUL!"

StartGetSVGAVBEInfoMessage:	db	"Start Get SVGA VBE Info"
GetSVGAVBEInfoErrMessage:	db	"Get SVGA VBE Info ERROR"
GetSVGAVBEInfoOKMessage:	db	"Get SVGA VBE Info SUCCESSFUL!"

StartGetSVGAModeInfoMessage:	db	"Start Get SVGA Mode Info"
GetSVGAModeInfoErrMessage:	db	"Get SVGA Mode Info ERROR"
GetSVGAModeInfoOKMessage:	db	"Get SVGA Mode Info SUCCESSFUL!"

SETVBEMODESUCCESSMESSAGE: db "Set VBE mode successful!"

STARTLOADKERNEL: db "Start Load kernel"
ENDLoadKERNEL: db "Load Kernel Successful"

