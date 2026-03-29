#[derive(Copy, Clone, Default)]
struct CharProperties {
    is_alpha: bool,
    is_digit: bool,
    is_ident: bool,
    is_space: bool,
}

const LOOKUP_TABLE: [CharProperties; 256] = {
    let mut table = [CharProperties {
        is_alpha: false,
        is_digit: false,
        is_ident: false,
        is_space: false,
    }; 256];

    let mut i = 0;
    while i < 256 {
        let c = i as u8;
        let mut props = CharProperties {
            is_alpha: c.is_ascii_alphabetic(),
            is_digit: c.is_ascii_digit(),
            is_ident: matches!(c, b'0'..=b'9' | b'a'..=b'z' | b'A'..=b'Z' | b' ' | b'_' | b'-'),
            is_space: matches!(c, b' ' | 0x09..=0x0D),
        };

        props.is_ident = props.is_digit || props.is_alpha || matches!(c, b'_' | b'-');

        table[i] = props;
        i += 1;
    }
    table
};

#[inline(always)]
#[must_use] 
pub const fn is_alpha(c: u8) -> bool {
    LOOKUP_TABLE[c as usize].is_alpha
}

#[inline(always)]
#[must_use] 
pub const fn is_digit(c: u8) -> bool {
    LOOKUP_TABLE[c as usize].is_digit
}

#[inline(always)]
#[must_use] 
pub const fn is_identifier(c: u8) -> bool {
    LOOKUP_TABLE[c as usize].is_ident
}

#[inline(always)]
#[must_use] 
pub const fn is_space(c: u8) -> bool {
    LOOKUP_TABLE[c as usize].is_space
}
