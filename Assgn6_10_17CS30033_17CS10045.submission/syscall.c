#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <debug.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);

// helper functions
static int get_byte(const uint8_t *unsigned_addr);
static bool put_byte (uint8_t *unsigned_dst, uint8_t byte);
static uint32_t get_word (const uint32_t *unsigned_addr);
static void user_input_validator(const uint8_t *unsigned_addr);

// a lock to provide mutual exclusion between system calls
static struct lock call_lock;

void
syscall_init (void) 
{
  // lock
  lock_init(&call_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// calling handlers with arguments required
static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t *esp = f->esp;
  // use switch case to invoke corresponding system call
  switch(get_word(esp))
  {
    case SYS_EXIT:
      sys_exit((int) get_word((esp)+1));
      break;
    case SYS_CREATE:
      f->eax = create((const char *) get_word((esp)+1), 
                            (unsigned) get_word((esp) + 2));
      break;
    case SYS_REMOVE:
      f->eax = remove((const char *) get_word((esp)+1));
      break;
    case SYS_OPEN:
      f->eax = open((const char *) get_word((esp)+1));
      break;
    case SYS_FILESIZE:
      f->eax = filesize((int) get_word((esp)+1));
      break;
    case SYS_READ:
      f->eax = read((int) get_word((esp)+1), 
                      (void *) get_word((esp)+2),
                      (unsigned) get_word((esp) + 3));
      break;
    case SYS_WRITE:
      f->eax = write((int) get_word((esp)+1), 
                      (void *) get_word((esp)+2),
                      (unsigned) get_word((esp) + 3));
      break;
    case SYS_CLOSE:
      close((int) get_word((esp)+1));
      break;
  }
  thread_exit ();
  // make sure this part is never reached
  NOT_REACHED();
}

// exit system call
void
sys_exit (int status){
  process_current()->info->exit_status = status;
  thread_exit();
}

// system call to create a file
static bool
create (const char *file, unsigned initial_size){
  // check if user input is valid
  user_input_validator(file);
  
  lock_acquire(&call_lock);
  if (filesys_create(file, initial_size)){
    lock_release(&call_lock);
    return true;
  }
  lock_release(&call_lock);
  return false;
}

// system call to remove file
static bool
remove (const char *file){
  // check if user input is valid
  user_input_validator(file);

  lock_acquire(&call_lock);
  if (filesys_remove(file)){
    lock_release(&call_lock);
    return true;
  }
  lock_release(&call_lock);
  return false;
}

// system call to open a file and return file descriptor
static int
open (const char *file){
  int fd;
  // check if user input is valid
  user_input_validator(file);

  lock_acquire(&call_lock);
  struct file *file_ = filesys_open(file);
  if (file_ == NULL){
    lock_release(&call_lock);
    return -1;
  }
  fd = get_descriptor(file_);
  lock_release(&call_lock);
  return false; 
}

// system call to get size of the file
static int
filesize (int fd){
  lock_acquire(&call_lock);

  proc_file *p_file_;
  int size;
  struct list *f_list = &process_current ()->file_list;
  struct list_elem *ele;
  for (ele=list_begin (f_list); ele != list_end (f_list); ele = list_next (ele)){
      p_file_ = list_entry (ele, proc_file, elem);
      if (p_file_->fd == fd)
        size = file_length(p_file_->file);
        lock_release(&call_lock);
        return size;
    }
  lock_release(&call_lock);  
  return -1;
}

// system call to read data from file descriptor
static int 
read (int fd, void *buffer, unsigned size){
  // check if user input is valid
  user_input_validator(buffer);
  int bytes = 0;
  uint8_t *buff = (uint8_t *) buffer;
  lock_acquire(&call_lock);

  if (fd==0){
    uint8_t buf;
    while(bytes<size && (buf=input_getc())!=0){
      *buff++ = buf;
      bytes++;
    }
    lock_release(&call_lock);
    return bytes;
  }

  struct file *file = get_file(fd);
  if (file != NULL){
    bytes = file_read(file, buffer, size);
    lock_release(&call_lock);
    return bytes;
  }
  lock_release(&call_lock);
  return -1;
}

// system call to write data to file descriptor
static int 
write (int fd, const void *buffer, unsigned size){
  // check if user input is valid
  user_input_validator(buffer);

  int bytes=-1;
  lock_acquire(&call_lock);

  if (fd == 1) {
    putbuf(buffer, size);
    lock_release(&call_lock);
    return size;
  }

  struct file *file = get_file(fd);
  if (file != NULL){
    bytes = file_write(file, buffer, size);
    lock_release(&call_lock);
    return bytes;
  }
  lock_release(&call_lock);
  return bytes;
}

// system call to close file descriptor
static void
close (int fd){
  lock_acquire(&call_lock);

  proc_file *p_file_;
  struct list *f_list = &process_current ()->file_list;
  struct list_elem *ele;
  for (ele=list_begin (f_list); ele != list_end (f_list); ele = list_next (ele)){
      p_file_ = list_entry (ele, proc_file, elem);
      if (p_file_->fd == fd)
        file_close(p_file_->file);
        list_remove(ele);
        free(p_file_);
        lock_release(&call_lock);
        return ;
    }
  lock_release(&call_lock);  
  return ;
}


/*
HELPER FUNCTIONS BELOW
*/

// Write a byte at user's virtual address provided as argument
static bool
put_byte(uint8_t *unsigned_dst, uint8_t byte){
  if (is_user_vaddr(unsigned_dst)){
    // catch error if any
    int error;
    asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error), "=m" (*unsigned_dst) : "q" (byte));
    if (error == -1) return false;
    return true;
  }
  return false;
}

// Read a word at at user's virtual address provided as argument
static uint32_t
get_word(const uint32_t *unsigned_addr){
  if (is_user_vaddr(unsigned_addr)){
    uint32_t result;
    int byte, i;
    // 4 byte words
    for (i=0;i<4;i++){
      byte = get_byte((uint8_t *)unsigned_addr+i);
      if (byte != -1){
        *((uint8_t *) &result+i) = (uint8_t) byte;
      }
      // failed to get byte
      sys_exit(-1);
    }
  }
  sys_exit(-1);
}

// Read a byte at user's virtual address provided as argument
static int
get_byte(const uint8_t *unsigned_addr){
  if (is_user_vaddr(unsigned_addr)){
    int result;
    asm("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (unsigned_addr));
    return result;
  }
  // when not user memory
  return -1;
}

static void
user_input_validator(const uint8_t *unsigned_addr){
  // exit if invalid input address
  if (get_byte(unsigned_addr) == -1) {
    sys_exit(-1);
  }
}