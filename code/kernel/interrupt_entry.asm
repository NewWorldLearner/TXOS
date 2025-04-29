[bits 64]
%define ERROR_CODE nop		 ; 若在相关的异常中cpu已经自动压入了错误码,为保持栈中格式统一,这里不做操作.
%define ZERO push 0		     ; 若在相关的异常中cpu没有压入错误码,为了统一栈中格式,就手工压入一个0

extern idt_func_table		 ;idt_func_table是C中注册的中断处理程序数组

section .data
global intr_entry_table
intr_entry_table:

%macro VECTOR 2
section .text
intr%1entry:		 ; 每个中断处理程序都要压入中断向量号,所以一个中断类型一个中断处理程序，自己知道自己的中断向量号是多少

   %2				 ; 中断若有错误码会压在eip后面 
; 以下是保存上下文环境
; 在64位系统中，段地址都固定为0，这样我们需要还需要对段寄存器进行压栈保护吗？为了以防万一，还是压一下吧
; 需要考虑切换数据段寄存器吗？

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    xor rax, rax
    mov ax, es
    push rax

    xor rax, rax
    mov ax, ds
    push rax

   mov rdi, rsp;                  ;将rsp的值传递给中断处理函数，中断处理函数通过[rsp+ 18 * 8]就可以知道发生中断时压入的rip地址了，因此就可以修改该值
   mov rsi, %1;                   ;%1是一个宏，表示的是中断向量号，将中断向量号传递给中断处理函数
   lea rax, [rel idt_func_table]  ;这里不使用call [idt_func_table + %1*8]来直接调用，原因似乎是nasm会使用32位地址来访问idt_func_table，导致错误
   call [rax + %1*8]              ;调用idt_func_table中的C版本中断处理函数，注意偏移量要乘以8



   jmp intr_exit

section .data
    dq    intr%1entry	 ; 存储各个中断入口程序的地址，形成intr_entry_table数组,注意64位中地址占8个字节
%endmacro


section .text
global intr_exit
intr_exit:	     
; 以下是恢复上下文环境
   ;add esp, 8			   ; 跳过中断号

   pop rax
   mov ds,ax
   pop rax
   mov es,ax

   pop r15
   pop r14
   pop r13
   pop r12
   pop r11
   pop r10
   pop r9
   pop r8
   pop rbp
   pop rdi
   pop rsi
   pop rdx
   pop rcx
   pop rbx
   pop rax

   add esp, 8			   ; 跳过error_code
   iretq                ; 注意64位下使用的指令是iretq

VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO 
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO 
VECTOR 0x08,ERROR_CODE
VECTOR 0x09,ZERO
VECTOR 0x0a,ERROR_CODE
VECTOR 0x0b,ERROR_CODE 
VECTOR 0x0c,ZERO
VECTOR 0x0d,ERROR_CODE
VECTOR 0x0e,ERROR_CODE
VECTOR 0x0f,ZERO 
VECTOR 0x10,ZERO
VECTOR 0x11,ERROR_CODE
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO 
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO 
VECTOR 0x18,ERROR_CODE
VECTOR 0x19,ZERO
VECTOR 0x1a,ERROR_CODE
VECTOR 0x1b,ERROR_CODE 
VECTOR 0x1c,ZERO
VECTOR 0x1d,ERROR_CODE
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO 
;下面的是外部中断，共24个外部中断
VECTOR 0x20,ZERO	;时钟中断对应的入口
VECTOR 0x21,ZERO	;键盘中断对应的入口
VECTOR 0x22,ZERO	;级联用的
VECTOR 0x23,ZERO	;串口2对应的入口
VECTOR 0x24,ZERO	;串口1对应的入口
VECTOR 0x25,ZERO	;并口2对应的入口
VECTOR 0x26,ZERO	;软盘对应的入口
VECTOR 0x27,ZERO	;并口1对应的入口
VECTOR 0x28,ZERO	;实时时钟对应的入口
VECTOR 0x29,ZERO	;重定向
VECTOR 0x2a,ZERO	;保留
VECTOR 0x2b,ZERO	;保留
VECTOR 0x2c,ZERO	;ps/2鼠标
VECTOR 0x2d,ZERO	;fpu浮点单元异常
VECTOR 0x2e,ZERO	;硬盘
VECTOR 0x2f,ZERO	;保留
VECTOR 0x30,ZERO	;保留
VECTOR 0x31,ZERO	;保留
VECTOR 0x32,ZERO	;保留
VECTOR 0x33,ZERO	;保留
VECTOR 0x34,ZERO	;保留
VECTOR 0x35,ZERO	;保留
VECTOR 0x36,ZERO	;保留
VECTOR 0x37,ZERO	;保留

