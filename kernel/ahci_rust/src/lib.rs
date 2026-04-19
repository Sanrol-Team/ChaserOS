//! ChaserOS AHCI（MMIO，假定内核 identity 映射低 4GiB，物理地址==内核虚拟地址）
#![no_std]
#![no_main]

use core::ptr::{addr_of, addr_of_mut, read_volatile, write_volatile};

#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! {
    loop {
        core::hint::spin_loop();
    }
}

const HBA_GHC: usize = 0x04;
const HBA_PI: usize = 0x0c;

const PORT_OFF: usize = 0x100;
const PORT_STRIDE: usize = 0x80;

const P_CLB: usize = 0x00;
const P_FB: usize = 0x08;
const P_IS: usize = 0x10;
const P_CMD: usize = 0x18;
const P_TFD: usize = 0x20;
const P_CI: usize = 0x38;

const PXCMD_ST: u32 = 1 << 0;
const PXCMD_FRE: u32 = 1 << 4;
const PXCMD_CR: u32 = 1 << 15;
const PXCMD_FR: u32 = 1 << 14;

const GHC_AE: u32 = 1 << 31;

#[repr(C, align(4096))]
struct AhciBuffers {
    cl: [u8; 1024],
    rfis: [u8; 256],
    ct: [u8; 256],
    data: [u8; 1024],
}

static mut BUF: AhciBuffers = AhciBuffers {
    cl: [0u8; 1024],
    rfis: [0u8; 256],
    ct: [0u8; 256],
    data: [0u8; 1024],
};

unsafe fn abar_read(abar: *const u32, off: usize) -> u32 {
    read_volatile(abar.byte_add(off))
}

unsafe fn abar_write(abar: *mut u32, off: usize, v: u32) {
    write_volatile(abar.byte_add(off).cast(), v);
}

unsafe fn port_base(abar: *mut u32, p: usize) -> *mut u32 {
    abar.byte_add(PORT_OFF + p * PORT_STRIDE).cast()
}

unsafe fn port_read(pb: *const u32, off: usize) -> u32 {
    read_volatile(pb.byte_add(off))
}

unsafe fn port_write(pb: *mut u32, off: usize, v: u32) {
    write_volatile(pb.byte_add(off).cast(), v);
}

fn spin_wait(mut f: impl FnMut() -> bool, max: u32) -> bool {
    for _ in 0..max {
        if f() {
            return true;
        }
        core::hint::spin_loop();
    }
    false
}

/// 置 GHC.AE，返回 PI（实现的端口位图）
#[no_mangle]
pub unsafe extern "C" fn chaseros_ahci_rust_init(abar: *mut u32) -> u32 {
    if abar.is_null() {
        return 0;
    }
    let ghc = abar_read(abar, HBA_GHC);
    abar_write(abar, HBA_GHC, ghc | GHC_AE);
    abar_read(abar, HBA_PI)
}

unsafe fn identify_to_buf(abar: *mut u32, port: u32) -> i32 {
    chaseros_ahci_rust_identify(abar, port, addr_of_mut!(BUF.data).cast::<u8>())
}

fn parse_identify_sectors(id: &[u8]) -> u32 {
    if id.len() < 512 {
        return 0;
    }
    let w60 = u16::from_le_bytes([id[120], id[121]]);
    let w61 = u16::from_le_bytes([id[122], id[123]]);
    let n28 = u32::from(w60) | (u32::from(w61) << 16);
    let n48 = u64::from(u16::from_le_bytes([id[200], id[201]]))
        | (u64::from(u16::from_le_bytes([id[202], id[203]])) << 16)
        | (u64::from(u16::from_le_bytes([id[204], id[205]])) << 32)
        | (u64::from(u16::from_le_bytes([id[206], id[207]])) << 48);
    let n48 = n48 & 0xFFFF_FFFF_FFFFu64;
    let lba48 = (u16::from_le_bytes([id[166], id[167]]) & 0x400) != 0
        && (u16::from_le_bytes([id[172], id[173]]) & 0x400) != 0;
    let total = if lba48 && n48 > 0 {
        n48
    } else if n28 > 0 && n28 <= 0x0FFF_FFFF {
        u64::from(n28)
    } else if n48 > 0 {
        n48
    } else {
        0u64
    };
    if total > u64::from(u32::MAX) {
        u32::MAX
    } else {
        total as u32
    }
}

/// 从 IDENTIFY 解析扇区总数（与 IDE 路径类似）
#[no_mangle]
pub unsafe extern "C" fn chaseros_ahci_rust_capacity_sectors(
    abar: *mut u32,
    port: u32,
    out: *mut u32,
) -> i32 {
    if abar.is_null() || out.is_null() {
        return -1;
    }
    if identify_to_buf(abar, port) != 0 {
        return -2;
    }
    let sec = parse_identify_sectors(BUF.data.split_at(512).0);
    if sec < 512 {
        return -3;
    }
    *out = sec;
    0
}

/// LBA28 读/写 1～2 个扇区（512×nsect 字节），`buf` 须可 DMA（identity 映射下任意内核指针）
#[no_mangle]
pub unsafe extern "C" fn chaseros_ahci_rust_rw_sectors(
    abar: *mut u32,
    port: u32,
    lba: u32,
    nsect: u32,
    buf: *mut u8,
    is_write: i32,
) -> i32 {
    if abar.is_null() || buf.is_null() || nsect == 0 || nsect > 2 {
        return -1;
    }
    if lba.saturating_add(nsect) > 0x1000_0000 {
        return -2;
    }
    let pi = abar_read(abar, HBA_PI);
    if port >= 32 || (pi & (1u32 << port)) == 0 {
        return -3;
    }
    let pb = port_base(abar, port as usize);
    let mut cmd = port_read(pb, P_CMD);
    cmd &= !(PXCMD_ST | PXCMD_FRE);
    port_write(pb, P_CMD, cmd);
    let _ = spin_wait(|| (port_read(pb, P_CMD) & (PXCMD_CR | PXCMD_FR)) == 0, 2_000_000);
    port_write(pb, P_IS, port_read(pb, P_IS));

    let nbytes = (nsect * 512) as usize;
    if is_write != 0 {
        core::ptr::copy_nonoverlapping(buf, addr_of_mut!(BUF.data).cast::<u8>(), nbytes);
    } else {
        core::ptr::write_bytes(addr_of_mut!(BUF.data).cast::<u8>(), 0, nbytes);
    }

    let cl_p = addr_of_mut!(BUF.cl) as usize as u64;
    let fb_p = addr_of_mut!(BUF.rfis) as usize as u64;
    let ct_p = addr_of_mut!(BUF.ct) as usize as u64;
    let data_p = addr_of_mut!(BUF.data) as usize as u64;

    core::ptr::write_bytes(addr_of_mut!(BUF.cl).cast::<u8>(), 0, 1024);
    core::ptr::write_bytes(addr_of_mut!(BUF.ct).cast::<u8>(), 0, 256);

    let cl = addr_of_mut!(BUF.cl).cast::<u32>();
    write_volatile(cl, 5 | (0u32 << 16));
    write_volatile(cl.add(1), 0);
    write_volatile(cl.add(2), ct_p as u32);
    write_volatile(cl.add(3), (ct_p >> 32) as u32);

    let ct = addr_of_mut!(BUF.ct).cast::<u8>();
    let ata_cmd = if is_write != 0 { 0x30u8 } else { 0x20u8 };
    *ct.add(0) = 0x27;
    *ct.add(1) = 0x80;
    *ct.add(2) = ata_cmd;
    *ct.add(3) = 0;
    *ct.add(4) = (lba & 0xFF) as u8;
    *ct.add(5) = ((lba >> 8) & 0xFF) as u8;
    *ct.add(6) = ((lba >> 16) & 0xFF) as u8;
    *ct.add(7) = (0x40u8 | ((lba >> 24) & 0x0F) as u8);
    *ct.add(12) = nsect as u8;

    let prdt = ct.add(0x80);
    core::ptr::write_unaligned(prdt as *mut u32, data_p as u32);
    core::ptr::write_unaligned(prdt.add(4) as *mut u32, (data_p >> 32) as u32);
    let dbc = (nbytes - 1) as u32 | (1u32 << 31);
    core::ptr::write_unaligned(prdt.add(12) as *mut u32, dbc);

    port_write(pb, P_CLB, cl_p as u32);
    port_write(pb, P_CLB + 4, (cl_p >> 32) as u32);
    port_write(pb, P_FB, fb_p as u32);
    port_write(pb, P_FB + 4, (fb_p >> 32) as u32);

    let mut c = port_read(pb, P_CMD);
    c |= PXCMD_FRE;
    port_write(pb, P_CMD, c);
    let _ = spin_wait(|| (port_read(pb, P_CMD) & PXCMD_FR) != 0, 2_000_000);
    let mut c2 = port_read(pb, P_CMD);
    c2 |= PXCMD_ST;
    port_write(pb, P_CMD, c2);
    let _ = spin_wait(|| (port_read(pb, P_CMD) & PXCMD_CR) != 0, 2_000_000);
    port_write(pb, P_IS, 0xFFFF_FFFF);
    port_write(pb, P_CI, 1);

    if !spin_wait(|| (port_read(pb, P_CI) & 1) == 0, 5_000_000) {
        return -4;
    }
    if (port_read(pb, P_TFD) & (1u32 << 0)) != 0 {
        return -5;
    }
    if is_write == 0 {
        core::ptr::copy_nonoverlapping(addr_of!(BUF.data).cast::<u8>(), buf, nbytes);
    }
    0
}

/// ATA IDENTIFY → 512 字节
#[no_mangle]
pub unsafe extern "C" fn chaseros_ahci_rust_identify(
    abar: *mut u32,
    port: u32,
    out512: *mut u8,
) -> i32 {
    if abar.is_null() || out512.is_null() {
        return -1;
    }
    let pi = abar_read(abar, HBA_PI);
    if port >= 32 || (pi & (1u32 << port)) == 0 {
        return -2;
    }

    let pb = port_base(abar, port as usize);

    let mut cmd = port_read(pb, P_CMD);
    cmd &= !(PXCMD_ST | PXCMD_FRE);
    port_write(pb, P_CMD, cmd);
    let _ = spin_wait(|| (port_read(pb, P_CMD) & (PXCMD_CR | PXCMD_FR)) == 0, 2_000_000);
    port_write(pb, P_IS, port_read(pb, P_IS));

    let cl_p = addr_of_mut!(BUF.cl) as usize as u64;
    let fb_p = addr_of_mut!(BUF.rfis) as usize as u64;
    let ct_p = addr_of_mut!(BUF.ct) as usize as u64;
    let data_p = addr_of_mut!(BUF.data) as usize as u64;

    core::ptr::write_bytes(addr_of_mut!(BUF.cl).cast::<u8>(), 0, 1024);
    core::ptr::write_bytes(addr_of_mut!(BUF.ct).cast::<u8>(), 0, 256);
    core::ptr::write_bytes(addr_of_mut!(BUF.data).cast::<u8>(), 0, 512);

    let cl = addr_of_mut!(BUF.cl).cast::<u32>();
    write_volatile(cl, 5 | (0u32 << 16));
    write_volatile(cl.add(1), 0);
    write_volatile(cl.add(2), ct_p as u32);
    write_volatile(cl.add(3), (ct_p >> 32) as u32);

    let ct = addr_of_mut!(BUF.ct).cast::<u8>();
    *ct.add(0) = 0x27;
    *ct.add(1) = 0x80;
    *ct.add(2) = 0xEC;
    *ct.add(3) = 0;
    *ct.add(12) = 1;

    let prdt = ct.add(0x80);
    core::ptr::write_unaligned(prdt as *mut u32, data_p as u32);
    core::ptr::write_unaligned(prdt.add(4) as *mut u32, (data_p >> 32) as u32);
    core::ptr::write_unaligned(prdt.add(12) as *mut u32, 511 | (1u32 << 31));

    port_write(pb, P_CLB, cl_p as u32);
    port_write(pb, P_CLB + 4, (cl_p >> 32) as u32);
    port_write(pb, P_FB, fb_p as u32);
    port_write(pb, P_FB + 4, (fb_p >> 32) as u32);

    let mut c = port_read(pb, P_CMD);
    c |= PXCMD_FRE;
    port_write(pb, P_CMD, c);
    let _ = spin_wait(|| (port_read(pb, P_CMD) & PXCMD_FR) != 0, 2_000_000);

    let mut c2 = port_read(pb, P_CMD);
    c2 |= PXCMD_ST;
    port_write(pb, P_CMD, c2);
    let _ = spin_wait(|| (port_read(pb, P_CMD) & PXCMD_CR) != 0, 2_000_000);

    port_write(pb, P_IS, 0xFFFF_FFFF);

    port_write(pb, P_CI, 1);

    let ok = spin_wait(|| (port_read(pb, P_CI) & 1) == 0, 5_000_000);
    if !ok {
        return -3;
    }
    if (port_read(pb, P_TFD) & (1u32 << 0)) != 0 {
        return -4;
    }

    core::ptr::copy_nonoverlapping(addr_of!(BUF.data).cast::<u8>(), out512, 512);
    0
}
