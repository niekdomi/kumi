bitflags::bitflags! {
    pub struct CharType: u8 {
        const DIGIT = 1 << 0;
        const ALPHA = 1 << 1;
        const IDENT = 1 << 2;
        const SPACE = 1 << 3;
    }
}

const fn compute_map() -> [u8; 256] {
    let mut table = [0u8; 256];
    let mut i = 0usize;
    while i < 256 {
        let c = i as u8;

        if c >= b'0' && c <= b'9' {
            table[i] |= CharType::DIGIT.bits();
            table[i] |= CharType::IDENT.bits();
        }

        if (c >= b'a' && c <= b'z') || (c >= b'A' && c <= b'Z') {
            table[i] |= CharType::ALPHA.bits();
            table[i] |= CharType::IDENT.bits();
        }

        if c == b'_' || c == b'-' {
            table[i] |= CharType::IDENT.bits();
        }

        if c == b' ' || (c >= 0x09 && c <= 0x0d) {
            table[i] |= CharType::SPACE.bits();
        }

        i += 1;
    }
    table
}

static LOOKUP_TABLE: [u8; 256] = compute_map();

#[inline(always)]
pub fn is_type(c: u8, flag: CharType) -> bool {
    (LOOKUP_TABLE[c as usize] & flag.bits()) != 0
}

#[inline(always)]
pub fn is_space(c: u8) -> bool {
    is_type(c, CharType::SPACE)
}

#[inline(always)]
pub fn is_digit(c: u8) -> bool {
    is_type(c, CharType::DIGIT)
}

#[inline(always)]
pub fn is_alpha(c: u8) -> bool {
    is_type(c, CharType::ALPHA)
}

#[inline(always)]
pub fn is_identifier(c: u8) -> bool {
    is_type(c, CharType::IDENT)
}
