# Giải thích chi tiết `user/sleep.c`

Chương trình `sleep` tạm dừng tiến trình trong một số **ticks** do người dùng nhập.
Một tick ≈ 10ms (xem `HƯỚNG DẪN THỰC HÀNH XV6.pdf`), tức `sleep 100` ≈ 1 giây.

Cú pháp: `sleep <ticks>`

---

## 1. Hai dòng `#include`

```c
#include "kernel/types.h"
#include "user/user.h"
```

| Dòng | File | Vì sao cần |
|---|---|---|
| 1 | `kernel/types.h` | Định nghĩa các kiểu cơ sở của xv6: `uint`, `ushort`, `uchar`, `uint64`… Phải include **trước** `user.h` vì `user.h` dùng `uint` trong một số nguyên mẫu (ví dụ `uint strlen(const char*)`). Đảo thứ tự hai dòng này là lỗi biên dịch. |
| 2 | `user/user.h` | Khai báo toàn bộ API mà chương trình người dùng được phép gọi: các system call (`sleep`, `exit`, `fork`…) và thư viện người dùng (`fprintf`, `atoi`, `strlen`…). Đây là file mình đã đọc để biết có `fprintf` mà dùng. |

Lưu ý: xv6 **không có** `stdio.h`, `stdlib.h`, `string.h`. Chương trình được biên dịch với `-nostdlib`, không có thư viện C chuẩn. Mọi thứ dùng được đều nằm trong `user/user.h`.

---

## 2. Hàm `parseticks` — kiểm tra và chuyển chuỗi thành số

```c
static int
parseticks(const char *s)
```

### Chữ ký hàm

| Thành phần | Ý nghĩa |
|---|---|
| `static` | Hàm chỉ tồn tại trong file `sleep.c` này, không xuất ra ngoài. Tránh trùng tên khi linker ghép `sleep.o` với `ulib.o`, `printf.o`… Đây là cách khai báo "hàm nội bộ" trong C. |
| `int` (trả về) | Trả về số ticks đã phân tích (≥ 0) nếu chuỗi hợp lệ, hoặc **`-1`** nếu không hợp lệ. Vì ticks hợp lệ luôn ≥ 0, giá trị âm dùng được làm mã lỗi mà không nhập nhằng. |
| `const char *s` | Con trỏ tới chuỗi đầu vào (chính là `argv[1]`). `const` tuyên bố hàm **không sửa** chuỗi — vừa là tài liệu cho người đọc, vừa để compiler chặn nếu mình vô tình gán vào `s[i]`. |

### Biến cục bộ

| Biến | Kiểu | Vai trò | Thời gian sống |
|---|---|---|---|
| `n` | `int` | Tích lũy giá trị số đang được xây dần từ trái sang phải | Từ lúc vào hàm đến lúc `return`, nằm trên stack |
| `s` | `const char *` | Vừa là tham số, vừa là con trỏ chạy — vòng `for` tăng chính nó để đi qua từng ký tự | Như trên |

### Giải thích từng dòng

**Dòng 10 — `int n = 0;`**
Khởi tạo bộ tích lũy. Bắt buộc gán `0`: biến cục bộ trong C **không** tự động bằng 0, nếu không khởi tạo nó chứa rác còn sót trên stack, và `n = n*10 + ...` sẽ cho kết quả ngẫu nhiên.

**Dòng 12–13 — chặn chuỗi rỗng**
```c
if(*s == '\0')
  return -1;
```
`*s` là ký tự đầu tiên. `'\0'` là byte kết thúc chuỗi của C. Nếu ký tự đầu đã là `'\0'` thì chuỗi rỗng (`sleep ""`). Không có dòng này, vòng `for` ở dưới chạy 0 lần và hàm trả về `n = 0` — tức `sleep ""` sẽ bị hiểu là `sleep 0`, một chuỗi rỗng lại được coi là hợp lệ. Đây là trường hợp biên dễ bỏ sót nhất.

**Dòng 14 — vòng lặp qua từng ký tự**
```c
for(; *s; s++){
```
- Phần khởi tạo của `for` để **trống** vì `s` đã có giá trị từ tham số, không cần khởi tạo lại.
- Điều kiện là `*s`, viết tắt của `*s != '\0'`. Trong C, giá trị `0` là *false*, khác `0` là *true*; ký tự `'\0'` có mã 0 nên biểu thức tự dừng đúng tại cuối chuỗi. Đây là lối viết chuẩn mực trong source xv6.
- `s++` dịch con trỏ sang ký tự kế tiếp mỗi vòng.

**Dòng 15–16 — chặn ký tự không phải chữ số**
```c
if(*s < '0' || *s > '9')
  return -1;
```
So sánh theo **mã ASCII**: `'0'` = 48 … `'9'` = 57, là một dải liên tiếp, nên chỉ cần kiểm hai đầu. Bất kỳ ký tự nào ngoài dải này (chữ cái, dấu `-`, dấu cách…) làm hàm trả `-1` ngay. Chính dòng này khiến `sleep abc` và `sleep 12abc` bị bắt lỗi.

**Dòng 17–18 — chặn tràn số nguyên**
```c
if(n > (0x7fffffff - (*s - '0')) / 10)
  return -1;
```
`0x7fffffff` = 2 147 483 647 = `INT_MAX`, giá trị lớn nhất của `int` 32-bit có dấu (xv6 không có `limits.h` nên phải viết thẳng hằng số).

Ý tưởng: thay vì nhân rồi xem có tràn không, ta **tính ngược** xem `n` tối đa là bao nhiêu thì phép `n*10 + d` còn vừa. Nếu `n` đã vượt ngưỡng đó thì dừng.

Vì sao phải kiểm **trước** khi nhân? Vì tràn số nguyên **có dấu** trong C là *undefined behavior* — compiler được phép giả định nó không bao giờ xảy ra, và với `-O` nó có thể **xóa luôn** phép kiểm tra kiểu `if(n < 0)` viết sau phép nhân. Kiểm trước là cách duy nhất đúng. Đây là dòng khiến `sleep 99999999999` báo lỗi thay vì ngủ một khoảng vô nghĩa.

**Dòng 19 — tích lũy chữ số**
```c
n = n * 10 + (*s - '0');
```
`*s - '0'` đổi ký tự thành giá trị số: `'7' - '0'` = 55 − 48 = 7. Nhân `n` với 10 để đẩy các chữ số cũ sang trái một hàng rồi cộng chữ số mới. Ví dụ `"123"`: n = 0 → 1 → 12 → 123.

**Dòng 21 — `return n;`**
Ra khỏi vòng lặp nghĩa là đã đi hết chuỗi mà không gặp lỗi nào, trả về giá trị đã dựng.

### Vì sao không dùng thẳng `atoi` như đề gợi ý?

`atoi` của xv6 (`user/ulib.c`) như sau:

```c
atoi(const char *s)
{
  int n;
  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}
```

Nó **dừng im lặng** ở ký tự đầu tiên không phải chữ số và trả về những gì đã đọc được. Hậu quả:

| Đầu vào | `atoi` trả | Vấn đề |
|---|---|---|
| `"abc"` | `0` | Không phân biệt được với `"0"` hợp lệ |
| `"12abc"` | `12` | Âm thầm bỏ qua phần rác |
| `"-5"` | `0` | Số âm bị coi như 0 |
| `""` | `0` | Chuỗi rỗng cũng thành 0 |

`atoi` không có kênh nào để báo lỗi, nên không thể dùng nó mà vẫn phát hiện input sai. Đây đúng là khuyết điểm kinh điển của `atoi` trong C chuẩn — lý do C hiện đại dùng `strtol` (có tham số `endptr` cho biết đã đọc tới đâu). xv6 không có `strtol`, nên phải tự viết `parseticks`.

`parseticks` vẫn **giữ nguyên thuật toán** của `atoi` (vòng `n*10 + chữ số`), chỉ thêm ba lớp kiểm tra. Nói cách khác: đề yêu cầu "đổi chuỗi sang số bằng atoi" và mình làm đúng việc đó, chỉ bổ sung phần báo lỗi mà `atoi` thiếu.

---

## 3. Hàm `main`

```c
int
main(int argc, char *argv[])
```

| Tham số | Ý nghĩa |
|---|---|
| `argc` | *argument count* — số đối số, **tính cả tên chương trình**. Gõ `sleep 10` thì `argc == 2`. |
| `argv` | *argument vector* — mảng con trỏ chuỗi. `argv[0]` = `"sleep"`, `argv[1]` = `"10"`. |

Ai đặt hai giá trị này? Shell của xv6 (`user/sh.c`) tách dòng lệnh thành các từ rồi gọi `exec("sleep", argv)`; kernel (`kernel/exec.c`) đẩy mảng đó lên stack của tiến trình mới.

### Biến cục bộ

| Biến | Kiểu | Vai trò |
|---|---|---|
| `ticks` | `int` | Giữ số ticks sau khi đã kiểm tra hợp lệ, để truyền cho system call |

### Giải thích từng dòng

**Dòng 29–32 — kiểm tra số lượng đối số**
```c
if(argc != 2){
  fprintf(2, "usage: sleep ticks\n");
  exit(1);
}
```
Dùng `!= 2` (chứ không phải `< 2`) nên **bắt cả hai hướng**: thiếu đối số (`sleep`) và thừa đối số (`sleep 10 20`). Thừa đối số mà im lặng bỏ qua thì người dùng không biết mình gõ sai.

`fprintf(2, ...)`: số `2` là **file descriptor 2 — stderr**. Quy ước UNIX: fd 0 = stdin, 1 = stdout, 2 = stderr. Thông báo lỗi phải ra fd 2 để khi người dùng chuyển hướng output (`sleep > f`) thì lỗi vẫn hiện trên màn hình. Nếu dùng `printf` (ghi vào fd 1) thì lỗi sẽ bị lẫn vào file output.

`exit(1)`: kết thúc với mã khác 0 = thất bại. Quy ước UNIX: 0 là thành công, khác 0 là lỗi. Nhờ vậy shell script kiểm tra được.

**Lưu ý — dòng này là bắt buộc, không phải trang trí.** Autograder của MIT (`grade-lab-util`, test *"sleep, no arguments"*) chạy `sleep` không đối số và **loại trừ** pattern `$ sleep\n$`, tức là chương trình **phải in ra gì đó**. Không in usage thì mất điểm test này.

**Dòng 34–37 — kiểm tra tính hợp lệ của đối số**
```c
if((ticks = parseticks(argv[1])) < 0){
  fprintf(2, "sleep: ticks phai la so nguyen khong am\n");
  exit(1);
}
```
Lối viết gộp: vừa gán `ticks` vừa so sánh kết quả, nhờ phép gán trong C trả về chính giá trị vừa gán. Cặp ngoặc trong `(ticks = ...) < 0` là **bắt buộc** — không có nó, C sẽ hiểu là `ticks = (parseticks(...) < 0)`, gán 0 hoặc 1 vào `ticks`, sai hoàn toàn. (Compiler cũng cảnh báo nếu thiếu ngoặc, và với `-Werror` thì thành lỗi luôn.)

Thông báo lỗi viết **không dấu** để hiển thị đúng trên console xv6 — console này chỉ xuất ASCII, không hỗ trợ UTF-8.

**Dòng 39–42 — gọi system call**
```c
if(sleep(ticks) < 0){
  fprintf(2, "sleep: sleep failed\n");
  exit(1);
}
```
`sleep(ticks)` là **system call**, không phải hàm thường. Đường đi của nó:

1. `user/usys.S` (sinh ra bởi `user/usys.pl`) đặt số hiệu `SYS_sleep` vào thanh ghi `a7` rồi thực thi lệnh `ecall`.
2. `ecall` gây trap, CPU chuyển sang chế độ supervisor, nhảy vào `kernel/trampoline.S` → `kernel/trap.c:usertrap()` → `kernel/syscall.c:syscall()`.
3. `syscall()` đọc `a7`, tra bảng `syscalls[]`, gọi `sys_sleep()` trong `kernel/sysproc.c`.

`sys_sleep` trả `-1` khi tiến trình bị `kill` giữa lúc đang ngủ, nên phải kiểm tra giá trị trả về.

**Dòng 44 — `exit(0);`**
Kết thúc thành công. Trong xv6, `exit()` được khai báo `__attribute__((noreturn))` — nó **không bao giờ** trả về, nên không cần (và không thể) viết `return 0;` sau nó. Đề bài yêu cầu rõ "Kết thúc bằng `exit(0)`".

Vì sao không `return 0`? Chương trình người dùng xv6 không có runtime C chuẩn để hứng giá trị trả về của `main`. `user/user.ld` đặt điểm vào thẳng ở `main`, nên `return` từ `main` sẽ nhảy vào một địa chỉ rác. Phải gọi `exit()` tường minh.

---

## 4. Phía kernel — `sys_sleep` làm gì

```c
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);              // lấy đối số thứ 0 từ thanh ghi của tiến trình
  if(n < 0)
    n = 0;
  acquire(&tickslock);        // khóa biến ticks toàn cục
  ticks0 = ticks;             // ghi lại thời điểm bắt đầu
  while(ticks - ticks0 < n){  // chưa đủ n ticks thì còn ngủ
    if(killed(myproc())){     // bị kill giữa lúc ngủ?
      release(&tickslock);
      return -1;              // -> chính là giá trị main kiểm tra
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}
```

Những điểm đáng chú ý:

- `argint(0, &n)` là cách kernel lấy đối số: tham số của system call nằm trong thanh ghi của tiến trình người dùng, kernel **không** đọc trực tiếp biến của userspace được.
- `tickslock` là spinlock bảo vệ biến đếm `ticks` toàn cục, vì ngắt timer (`kernel/trap.c:clockintr`) tăng `ticks` song song trên CPU khác.
- `sleep(&ticks, &tickslock)` là **hàm sleep của kernel**, khác hẳn system call `sleep` của userspace dù trùng tên. Nó đưa tiến trình vào trạng thái `SLEEPING` trên "channel" `&ticks` và **nhả lock** trong lúc ngủ. Ngắt timer gọi `wakeup(&ticks)` mỗi tick để đánh thức.
- Vòng `while` chứ không phải `if`: tiến trình bị đánh thức mỗi tick, phải tự kiểm tra xem đã đủ `n` ticks chưa rồi ngủ lại. Mẫu "ngủ trong vòng lặp kiểm tra điều kiện" này là chuẩn mực tránh *spurious wakeup*.
- Vì kernel đã tự kẹp `n < 0` về `0`, một `sleep(-5)` sẽ không treo máy. Nhưng mình vẫn chặn số âm ở userspace để **báo cho người dùng biết họ gõ sai**, chứ không âm thầm làm việc khác ý họ.

---

## 5. Luồng chạy đầy đủ

Trường hợp `sleep 10` chạy đúng:

```
Người dùng gõ: sleep 10
        |
        v
sh (user/sh.c) tách thành ["sleep", "10"], fork + exec
        |
        v
kernel/exec.c nạp ELF user/_sleep từ fs.img, dựng stack chứa argc=2, argv
        |
        v
main() bắt đầu:  argc = 2, argv[0] = "sleep", argv[1] = "10"
        |
        +--> argc != 2 ?  KHÔNG (bằng 2)  --> đi tiếp
        |
        +--> parseticks("10"):
        |       *s = '1' -> không rỗng, là chữ số, không tràn -> n = 1
        |       *s = '0' -> là chữ số, không tràn             -> n = 10
        |       *s = '\0' -> hết vòng lặp                     -> return 10
        |    ticks = 10, không < 0  --> đi tiếp
        |
        +--> sleep(10)
        |       a7 = SYS_sleep, ecall -> trap vào kernel
        |       sys_sleep: ticks0 = ticks, ngủ cho đến khi ticks - ticks0 >= 10
        |       (khoảng 100ms; shell không in gì, trông như treo)
        |       trả về 0
        |    0 không < 0  --> đi tiếp
        |
        v
exit(0) -> tiến trình kết thúc, sh in lại dấu nhắc $
```

Các nhánh lỗi:

| Lệnh | Nhánh rẽ ở đâu | Kết quả |
|---|---|---|
| `sleep` | dòng 29, `argc == 1 != 2` | `usage: sleep ticks`, exit 1 |
| `sleep 10 20` | dòng 29, `argc == 3 != 2` | `usage: sleep ticks`, exit 1 |
| `sleep abc` | dòng 15, `'a'` không phải chữ số | `sleep: ticks phai la so nguyen khong am`, exit 1 |
| `sleep 12abc` | dòng 15, gặp `'a'` sau khi đã đọc `12` | như trên, exit 1 |
| `sleep -5` | dòng 15, `'-'` không phải chữ số | như trên, exit 1 |
| `sleep ""` | dòng 12, chuỗi rỗng | như trên, exit 1 |
| `sleep 99999999999` | dòng 17, tràn `int` | như trên, exit 1 |
| `sleep 0` | không rẽ | `sleep(0)` trả về ngay, exit 0 |

---

## 6. Kiểm thử đã thực hiện

**Autograder** — `./grade-lab-util sleep`, cả 3 test đạt (20/20 điểm):

```
== Test sleep, no arguments == sleep, no arguments: OK (1.6s)
== Test sleep, returns == sleep, returns: OK (0.8s)
== Test sleep, makes syscall == sleep, makes syscall: OK (0.9s)
```

**Thủ công trong QEMU:**

```
$ sleep
usage: sleep ticks
$ sleep abc
sleep: ticks phai la so nguyen khong am
$ sleep 12abc
sleep: ticks phai la so nguyen khong am
$ sleep -5
sleep: ticks phai la so nguyen khong am
$ sleep 99999999999
sleep: ticks phai la so nguyen khong am
$ sleep 0
$ sleep 3
$ echo DONE
DONE
```

---

## 7. Khó khăn và ghi chú cho báo cáo

**1. Ký tự đầu tiên bị console xv6 ăn mất.** Khi test tự động bằng cách đẩy lệnh qua stdin của QEMU, lệnh đầu tiên luôn bị mất ký tự đầu — gõ `sleep` thì shell báo `exec leep failed`, gõ `echo warmup` thì báo `exec cho failed`. Ban đầu tưởng chương trình chưa được đưa vào `fs.img`. Thực ra đây là đặc tính của console xv6: ký tự gửi tới trong lúc UART/shell còn đang khởi tạo bị mất. **Không phải lỗi code.** Cách xử lý: chèn một lệnh "mồi" vô hại ở đầu danh sách test để nó hứng phần mất mát.

**2. Môi trường build — GCC quá mới.** Máy dùng macOS với GCC 16 từ Homebrew, trong khi xv6-labs-2024 được viết cho GCC 10–13. Phải thêm một dòng vào `Makefile`:

```make
CFLAGS += -std=gnu17 -Wno-unused-but-set-variable
```

Lý do: từ GCC 14, mặc định là chuẩn C23, trong đó `()` trong khai báo hàm nghĩa là `(void)`. File `user/usertests.c` **của MIT** khai báo `rwsbrk()` theo kiểu K&R cũ rồi lưu vào bảng `void (*)(char *)`, nên C23 coi là lỗi kiểu con trỏ. Cộng với `-Werror`, build dừng hẳn — dù đây là file test của MIT, không phải code nhóm viết. Đây là **sửa môi trường, không phải phần của bài tập**, nhưng nó sẽ xuất hiện trong file `.patch` nên cần nêu trong báo cáo. Hai flag này vô hại trên GCC của Linux (`-std=gnu17` được hỗ trợ từ GCC 8).

**3. Quyết định thiết kế: kiểm tra đầu vào chặt hơn đề yêu cầu.** Đề chỉ nói "đổi chuỗi sang số bằng `atoi`". Nhóm chọn tự viết `parseticks` vì `atoi` không có cách nào báo lỗi (xem mục 2). Thuật toán lõi vẫn giống `atoi`, chỉ thêm ba lớp kiểm tra: chuỗi rỗng, ký tự không phải chữ số, và tràn `int`. Đánh đổi: thêm ~12 dòng code, bù lại không âm thầm hiểu sai ý người dùng.

**4. Khác biệt giữa `make` và `make qemu`.** `fs.img` **không** nằm trong target mặc định của `make`. Lần đầu chạy `make` thành công nhưng không có `fs.img` nên không test được gì. Phải chạy `make qemu`, `make grade`, hoặc gọi thẳng `make fs.img`.
