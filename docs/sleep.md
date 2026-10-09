# user/sleep.c — thiết kế và luồng chạy

> Giải thích từng dòng nằm ngay trong comment của `user/sleep.c`.
> Tài liệu này ghi phần **vì sao** chọn cách làm đó, luồng chạy lúc thực thi, và kết quả kiểm thử.
> Tham chiếu theo tên bước ghi trong code (`Bước 1`, `Bước 2a`…) thay vì số dòng, để sửa code không làm hỏng tài liệu.

## Chức năng

`sleep <số_tick>` — tạm dừng tiến trình rồi thoát. Một tick là một lần ngắt đồng hồ, khoảng 10ms, nên `sleep 100` dừng khoảng một giây.

## Cấu trúc file

| Thành phần | Vai trò |
|---|---|
| 2 dòng `#include` | `types.h` cho kiểu cơ sở, `user.h` cho system call và thư viện |
| `#define INT_MAX` | xv6 không có `<limits.h>` |
| `parse_ticks()` | Đổi chuỗi sang số, có kiểm tra hợp lệ |
| `main()` | Kiểm đối số, gọi `parse_ticks`, gọi system call `sleep` |

Thứ tự hai dòng include không được đảo: `user.h` dùng kiểu `uint` trong một số khai báo, mà `uint` định nghĩa trong `types.h`.

---

## Quyết định thiết kế 1 — không dùng thẳng `atoi`

Đề gợi ý dùng `atoi`. Toàn bộ thân hàm đó trong `user/ulib.c`:

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

Nó dừng im lặng ở ký tự đầu tiên không phải chữ số và trả về phần đã đọc. Không có kênh nào báo lỗi:

| Đầu vào | Trả về | Vấn đề |
|---|---|---|
| `"10"` | 10 | đúng |
| `"abc"` | 0 | không phân biệt được với `"0"` hợp lệ |
| `"12abc"` | 12 | âm thầm bỏ phần rác |
| `"-5"` | 0 | số âm thành 0 |
| `""` | 0 | chuỗi rỗng thành 0 |

Hệ quả: `sleep abc` ngủ 0 tick rồi thoát, không báo gì.

`parse_ticks` **giữ nguyên thuật toán** của `atoi` (vòng `n*10 + chữ số`), chỉ thêm ba lớp kiểm tra và dùng giá trị âm làm mã lỗi. Dùng được giá trị âm vì số tick hợp lệ luôn không âm.

Đây là khuyết điểm kinh điển của `atoi` trong C chuẩn, lý do C hiện đại dùng `strtol` có tham số `endptr`. xv6 không có `strtol`.

## Quyết định thiết kế 2 — kiểm tràn trước khi nhân

Bước `2c` trong code. Thay vì nhân rồi xem kết quả có âm không, biến đổi bất đẳng thức:

```
ket_qua * 10 + chu_so  <=  INT_MAX
ket_qua * 10           <=  INT_MAX - chu_so
ket_qua                <=  (INT_MAX - chu_so) / 10
```

Bắt buộc kiểm **trước**. Tràn số nguyên có dấu trong C là *undefined behavior*: trình biên dịch được phép giả định nó không xảy ra, và với `-O` (Makefile xv6 có bật) nó sẽ **xóa luôn** phép kiểm tra viết sau phép nhân, vì coi đó là điều kiện không bao giờ đúng.

## Quyết định thiết kế 3 — `argc != 2` thay vì `argc < 2`

Bắt được cả hai hướng sai:

| Lệnh | `argc` | Kết quả |
|---|---|---|
| `sleep` | 1 | thiếu đối số → báo lỗi |
| `sleep 10` | 2 | hợp lệ |
| `sleep 10 20` | 3 | thừa đối số → báo lỗi |

Chỉ kiểm `< 2` thì trường hợp thừa bị bỏ qua trong im lặng.

## Quyết định thiết kế 4 — lỗi ra fd 2

Quy ước UNIX: fd 0 là stdin, 1 là stdout, 2 là stderr. Dùng `fprintf(2, ...)` nên khi người dùng chuyển hướng kết quả (`sleep > f`) thì lỗi vẫn hiện trên màn hình.

Việc in thông báo khi thiếu đối số còn là **điều kiện chấm điểm**: test `sleep, no arguments` trong `grade-lab-util` loại trừ trường hợp chương trình không in gì.

Thông báo viết không dấu vì console xv6 chỉ xuất ASCII.

## Quyết định thiết kế 5 — `exit(0)` chứ không `return 0`

Chương trình user của xv6 không có runtime C chuẩn để hứng giá trị trả về của `main`. File `user/user.ld` đặt điểm vào thẳng ở `main`, nên `return` sẽ nhảy vào địa chỉ rác. `exit` còn khai báo `__attribute__((noreturn))` nên không cần viết gì phía sau.

---

## Luồng chạy

Trường hợp `sleep 25`:

```
sh tách "sleep 25" → argc=2, argv[0]="sleep", argv[1]="25"
   │
   ├─ main Bước 1: argc != 2 ?  2 != 2 → sai, đi tiếp
   │
   ├─ main Bước 2: parse_ticks("25")
   │     ├─ Bước 1:  s[0]='2' khác '\0'      → không trả -1
   │     ├─ Bước 2 vòng i=0:  '2' hợp lệ, chu_so=2
   │     │     2c: 0 > (2147483647-2)/10 ?  không
   │     │     2d: ket_qua = 0*10+2 = 2
   │     ├─ Bước 2 vòng i=1:  '5' hợp lệ, chu_so=5
   │     │     2d: ket_qua = 2*10+5 = 25
   │     └─ s[2]='\0' → thoát vòng, return 25
   │     so_tick = 25, không < 0 → đi tiếp
   │
   ├─ main Bước 3: sleep(25)
   │     usys.S:  li a7, SYS_sleep → ecall
   │     kernel:  trap.c → syscall.c → sysproc.c:sys_sleep
   │     sys_sleep ngủ ~250ms, trả 0
   │     0 không < 0 → đi tiếp
   │
   └─ main Bước 4: exit(0)
```

Các nhánh lỗi:

| Lệnh | Dừng ở | Thông báo |
|---|---|---|
| `sleep` | main Bước 1 | `usage: sleep ticks` |
| `sleep 10 20` | main Bước 1 | `usage: sleep ticks` |
| `sleep ""` | parse Bước 1 | `ticks phai la so nguyen khong am` |
| `sleep abc` | parse Bước 2a | như trên |
| `sleep 12abc` | parse Bước 2a (sau khi đọc `12`) | như trên |
| `sleep -5` | parse Bước 2a (ký tự `-`) | như trên |
| `sleep 99999999999` | parse Bước 2c | như trên |
| `sleep 0` | không dừng | thoát ngay, không in gì |
| `sleep 007` | không dừng | ngủ 7 tick |

---

## Phía kernel — `sys_sleep` trong `kernel/sysproc.c`

```c
argint(0, &n);              // lấy đối số từ thanh ghi của tiến trình
if(n < 0) n = 0;
acquire(&tickslock);
ticks0 = ticks;
while(ticks - ticks0 < n){
  if(killed(myproc())){ release(&tickslock); return -1; }
  sleep(&ticks, &tickslock);
}
release(&tickslock);
return 0;
```

- `argint` là cách kernel lấy đối số. Kernel **không** đọc trực tiếp biến của userspace được; đối số nằm trong thanh ghi của tiến trình.
- `tickslock` bảo vệ biến đếm `ticks` toàn cục, vì ngắt timer (`kernel/trap.c:clockintr`) tăng nó song song trên CPU khác.
- `sleep(&ticks, &tickslock)` là **hàm sleep của kernel**, khác hẳn system call `sleep` của userspace dù trùng tên. Nó đưa tiến trình vào trạng thái `SLEEPING` và **nhả lock** trong lúc ngủ.
- Vòng `while` chứ không phải `if`: tiến trình bị đánh thức mỗi tick, phải tự kiểm đã đủ chưa rồi ngủ lại. Mẫu "ngủ trong vòng kiểm điều kiện" này lặp lại ở `kernel/pipe.c` mà bài `pingpong` dùng.
- Kernel tự kẹp `n < 0` về `0`, nên `sleep(-5)` không treo máy. Nhưng vẫn chặn số âm ở userspace để **báo cho người dùng biết họ gõ sai**.
- Trả `-1` khi tiến trình bị `kill` giữa lúc ngủ — chính giá trị `main` kiểm tra ở Bước 3.

---

## Kiểm thử

Autograder, `./grade-lab-util sleep` — 3/3 đạt (20/20 điểm):

```
== Test sleep, no arguments == sleep, no arguments: OK
== Test sleep, returns == sleep, returns: OK
== Test sleep, makes syscall == sleep, makes syscall: OK
```

Thủ công trong shell xv6:

```
$ sleep
usage: sleep ticks
$ sleep abc
sleep: ticks phai la so nguyen khong am
$ sleep 5x
sleep: ticks phai la so nguyen khong am
$ sleep -5
sleep: ticks phai la so nguyen khong am
$ sleep 99999999999
sleep: ticks phai la so nguyen khong am
$ sleep 10 20
usage: sleep ticks
$ sleep 007
$ echo xong
xong
```

`sleep 007` không in gì và thoát ngay — đúng: hai số 0 đầu không ảnh hưởng vì `0*10+0 = 0`, kết quả là 7 tick.

Chạy lại bộ test này bằng:

```sh
python3 xv6run.py 'sleep' 'sleep abc' 'sleep 5x' 'sleep -5' 'sleep 99999999999' 'sleep 10 20' 'sleep 007' 'echo xong'
```

---

## Khó khăn và ghi chú cho báo cáo

**1. Ký tự đầu tiên bị console xv6 bỏ mất.** Khi đẩy lệnh qua stdin của QEMU, lệnh đầu tiên mất ký tự đầu: gõ `sleep` thì shell báo `exec leep failed`. Ban đầu tưởng chương trình chưa vào `fs.img`. Nguyên nhân thật: ký tự gửi tới trong lúc UART và shell còn khởi tạo thì bị mất. **Đã khắc phục** bằng cách cho script chờ 3 giây sau khi boot rồi mới gửi lệnh đầu tiên — không còn mất ký tự nữa.

**2. Môi trường build — GCC quá mới.** macOS với GCC 16 từ Homebrew, trong khi xv6-labs-2024 viết cho GCC 10–13. Phải thêm vào `Makefile`:

```make
CFLAGS += -std=gnu17 -Wno-unused-but-set-variable
```

Từ GCC 14 mặc định là C23, trong đó `()` trong khai báo hàm nghĩa là `(void)`. File `user/usertests.c` **của MIT** khai báo `rwsbrk()` kiểu K&R rồi lưu vào bảng `void (*)(char *)`, C23 coi là lỗi kiểu. Cộng `-Werror` nên build dừng hẳn. Đây là sửa môi trường, không phải phần bài tập, nhưng sẽ xuất hiện trong `.patch`.

**3. `make` khác `make qemu`.** `fs.img` không nằm trong target mặc định. Lần build đầu `make` thành công nhưng không có `fs.img` nên không test được gì.

**4. Quyết định kiểm tra chặt hơn đề.** Đề chỉ nói dùng `atoi`. Nhóm thêm ~15 dòng kiểm tra. Đánh đổi: code dài hơn, bù lại không âm thầm hiểu sai ý người dùng. Đề chấm cả "phong cách lập trình" nên phần này đáng làm.
