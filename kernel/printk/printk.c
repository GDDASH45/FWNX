
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

#define VGA_COLOR  0x0F

static size_t cursor_x;
static size_t cursor_y;

static void vga_putentry(char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;

    VGA_MEMORY[index] =
        (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void vga_scroll(void)
{
    size_t x;
    size_t y;

    for (y = 1; y < VGA_HEIGHT; y++) {
        for (x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] =
                VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }

    for (x = 0; x < VGA_WIDTH; x++) {
        vga_putentry(' ', VGA_COLOR, x, VGA_HEIGHT - 1);
    }

    cursor_y = VGA_HEIGHT - 1;
}

static void printk_putc(char c)
{
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_putentry(' ', VGA_COLOR, cursor_x, cursor_y);
        }
    } else {
        vga_putentry(c, VGA_COLOR, cursor_x, cursor_y);
        cursor_x++;

        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }
}

static void printk_puts(const char *s)
{
    while (*s) {
        printk_putc(*s++);
    }
}

static void printk_put_unsigned(unsigned int value, unsigned int base)
{
    char buffer[32];
    size_t length = 0;
    const char digits[] = "0123456789abcdef";

    if (base < 2 || base > 16) {
        printk_puts("?");
        return;
    }

    do {
        buffer[length++] = digits[value % base];
        value /= base;
    } while (value != 0 && length < sizeof(buffer));

    while (length > 0) {
        printk_putc(buffer[--length]);
    }
}

static void printk_put_signed(int value)
{
    unsigned int magnitude;

    if (value < 0) {
        printk_putc('-');
        magnitude = 0u - (unsigned int)value;
    } else {
        magnitude = (unsigned int)value;
    }

    printk_put_unsigned(magnitude, 10);
}

void printk(const char *format, ...)
{
    va_list args;

    va_start(args, format);

    while (*format) {
        if (*format != '%') {
            printk_putc(*format++);
            continue;
        }

        format++;

        switch (*format) {
        case '%':
            printk_putc('%');
            break;

        case 'c':
            printk_putc((char)va_arg(args, int));
            break;

        case 's': {
            const char *s = va_arg(args, const char *);

            printk_puts(s ? s : "(null)");
            break;
        }

        case 'd':
        case 'i':
            printk_put_signed(va_arg(args, int));
            break;

        case 'u':
            printk_put_unsigned(va_arg(args, unsigned int), 10);
            break;

        case 'x':
        case 'p':
            printk_put_unsigned(va_arg(args, unsigned int), 16);
            break;

        case 'X': {
            unsigned int value = va_arg(args, unsigned int);
            char buffer[32];
            size_t length = 0;
            const char digits[] = "0123456789ABCDEF";

            do {
                buffer[length++] = digits[value % 16];
                value /= 16;
            } while (value != 0 && length < sizeof(buffer));

            while (length > 0) {
                printk_putc(buffer[--length]);
            }

            break;
        }

        case '\0':
            va_end(args);
            return;

        default:
            printk_putc('%');
            printk_putc(*format);
            break;
        }

        format++;
    }

    va_end(args);
}
