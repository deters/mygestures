use std::ffi::CString;
use std::ptr;

#[repr(C)]
struct ShmMessage {
    pid: i32,
    kill: i32,
}

pub struct DaemonIpc {
    shm_name: String,
    ptr: *mut ShmMessage,
}

impl DaemonIpc {
    pub fn new(device_name: &str) -> Result<Self, std::io::Error> {
        let sanitized = device_name.replace('/', "%");
        let shm_name = format!(
            "/mygestures_uid_{}_dev_{}",
            unsafe { libc::getuid() },
            sanitized
        );
        let c_name = CString::new(shm_name.clone()).unwrap();

        let fd = unsafe { libc::shm_open(c_name.as_ptr(), libc::O_CREAT | libc::O_RDWR, 0o600) };
        if fd < 0 {
            return Err(std::io::Error::last_os_error());
        }

        let size = std::mem::size_of::<ShmMessage>();
        if unsafe { libc::ftruncate(fd, size as libc::off_t) } < 0 {
            let err = std::io::Error::last_os_error();
            unsafe {
                libc::close(fd);
            }
            return Err(err);
        }

        let ptr = unsafe {
            libc::mmap(
                ptr::null_mut(),
                size,
                libc::PROT_READ | libc::PROT_WRITE,
                libc::MAP_SHARED,
                fd,
                0,
            )
        };
        unsafe {
            libc::close(fd);
        }

        if ptr == libc::MAP_FAILED {
            return Err(std::io::Error::last_os_error());
        }

        Ok(DaemonIpc {
            shm_name,
            ptr: ptr as *mut ShmMessage,
        })
    }

    pub fn send_kill_message(&self) {
        unsafe {
            let msg = &mut *self.ptr;
            if msg.pid > 0 {
                println!("Asking mygestures running on pid {} to exit..", msg.pid);
                let running_pid = msg.pid;
                msg.pid = libc::getpid();
                msg.kill = 1;
                let _ = libc::kill(running_pid, libc::SIGINT);
                std::thread::sleep(std::time::Duration::from_millis(100));
            }
            msg.pid = libc::getpid();
            msg.kill = 0;
        }
    }

    pub fn is_kill_requested(&self) -> bool {
        unsafe {
            let msg = &*self.ptr;
            msg.kill != 0
        }
    }

    pub fn get_active_pid(&self) -> i32 {
        unsafe {
            let msg = &*self.ptr;
            msg.pid
        }
    }
}

impl Drop for DaemonIpc {
    fn drop(&mut self) {
        let size = std::mem::size_of::<ShmMessage>();
        unsafe {
            let _ = libc::munmap(self.ptr as *mut libc::c_void, size);
            let c_name = CString::new(self.shm_name.clone()).unwrap();
            let _ = libc::shm_unlink(c_name.as_ptr());
        }
    }
}
