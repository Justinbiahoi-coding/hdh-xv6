# Giải thích chi tiết `user/pingpong.c`

Chương trình cho hai tiến trình cha–con truyền qua lại **1 byte**: cha gửi sang con ("ping"), con gửi trả lại cha ("pong"). Mục đích là làm quen với `pipe`, `fork`, `read`, `write`, `getpid`.

Output mong đợi:

```
$ pingpong
4: received ping
3: received pong
$
```

PID con luôn lớn hơn PID cha, vì con được tạo sau.

---

## 1. Câu hỏi quan trọng nhất: vì sao phải **hai** pipe?

Đây là chỗ dễ làm sai nhất của bài, nên giải thích trước khi đọc code.

### Pipe trong xv6 là gì

`pipe(int fd[2])` tạo một **bộ đệm vòng trong kernel** (`struct pipe`, kích thước `PIPESIZE` = 512 byte) và trả về hai file descriptor:

| | Ý nghĩa |
|---|---|
| `fd[0]` | **Đầu đọc** — đọc dữ liệu ra khỏi pipe |
| `fd[1]` | **Đầu ghi** — đẩy dữ liệu vào pipe |

Pipe là **một chiều**: dữ liệu chỉ chảy từ `fd[1]` sang `fd[0]`.

### Vấn đề: `fork()` nhân đôi bảng file descriptor

Sau `fork()`, tiến trình con có **bản sao** bảng fd của cha. Nghĩa là nếu chỉ có một pipe:

```
            pipe A
          /        \
   cha: A[0] A[1]   con: A[0] A[1]
        đọc  ghi         đọc  ghi
```

**Cả bốn** đầu đều mở. Cha ghi byte vào `A[1]`, nhưng ngay sau đó chính cha vẫn giữ `A[0]` và có thể đọc lại byte của chính mình. Hai tiến trình chạy song song nên thứ tự không xác định:

- Nếu con `read` trước → chạy đúng
- Nếu cha `read` trước → cha tự ăn byte của mình, con bị treo vĩnh viễn chờ dữ liệu không bao giờ tới

Đây là **race condition**: chương trình chạy đúng lúc này, sai lúc khác, và lỗi không tái hiện đều. Loại lỗi khó debug nhất.

### Giải pháp: hai pipe + đóng đầu không dùng

Dùng hai pipe, mỗi pipe một chiều cố định, rồi mỗi tiến trình **đóng** hai đầu nó không cần:

```
      p2c (ping: cha -> con)            c2p (pong: con -> cha)
      
  cha: [0] ĐÓNG   [1] ghi           cha: [0] đọc   [1] ĐÓNG
  con: [0] đọc    [1] ĐÓNG          con: [0] ĐÓNG  [1] ghi
```

Sau khi đóng, mỗi chiều chỉ còn **một** người ghi và **một** người đọc. Không còn đường nào để một tiến trình đọc lại byte của chính nó. Race biến mất **về mặt cấu trúc**, không phải nhờ may mắn về thời điểm.

---

## 2. Khai báo và biến

```c
#include "kernel/types.h"
#include "user/user.h"
```

Giống `sleep.c`: `types.h` cho các kiểu cơ sở, phải đứng trước `user.h`; `user.h` khai báo `pipe`, `fork`, `read`, `write`, `close`, `getpid`, `wait`, `printf`, `fprintf`, `exit`.

```c
int
main(void)
```

Dùng `main(void)` thay vì `main(int argc, char *argv[])` vì `pingpong` **không nhận đối số nào**. Khai báo tham số rồi không dùng là nhiễu cho người đọc.

### Bảng biến

| Biến | Kiểu | Vai trò |
|---|---|---|
| `p2c` | `int[2]` | Pipe chiều **cha → con**. `p2c[0]` đầu đọc, `p2c[1]` đầu ghi. Tên viết tắt của *parent to child*. |
| `c2p` | `int[2]` | Pipe chiều **con → cha** (*child to parent*). |
| `buf` | `char` | Ô chứa đúng 1 byte được truyền. Không cần mảng vì chỉ truyền 1 byte; truyền địa chỉ bằng `&buf`. |
| `pid` | `int` | Giá trị `fork()` trả về. **Đây là biến phân biệt cha với con** — xem mục 4. |

Vì sao `p2c` và `c2p` là mảng 2 phần tử? Vì `pipe()` cần ghi **hai** fd ra ngoài, mà C chỉ cho hàm trả về một giá trị. Cách giải quyết: truyền vào con trỏ tới mảng để hàm ghi trực tiếp vào đó. `int p2c[2]` khi truyền vào `pipe(p2c)` tự suy biến thành con trỏ `int*` trỏ tới phần tử đầu.

---

## 3. Tạo hai pipe (dòng 19–22)

```c
if(pipe(p2c) < 0 || pipe(c2p) < 0){
  fprintf(2, "pingpong: pipe failed\n");
  exit(1);
}
```

`pipe()` trả `0` khi thành công, `-1` khi thất bại (hết file descriptor, hoặc hết bộ nhớ kernel cho `struct pipe`).

Lưu ý về **short-circuit** của `||`: nếu `pipe(p2c)` đã thất bại thì C **không** thực thi `pipe(c2p)`. Đúng ý muốn — pipe đầu lỗi thì không cần tạo pipe thứ hai.

Hai pipe này được tạo **trước** `fork()`. Bắt buộc phải vậy: chỉ những fd đã mở trước khi fork mới được con thừa hưởng. Tạo pipe sau fork thì mỗi tiến trình có pipe riêng, không nối được với nhau.

Sau hai dòng này, tiến trình có 4 fd mới. Giả sử fd 0,1,2 đã dùng cho stdin/stdout/stderr thì:

| fd | Là gì |
|---|---|
| 3 | `p2c[0]` |
| 4 | `p2c[1]` |
| 5 | `c2p[0]` |
| 6 | `c2p[1]` |

---

## 4. `fork()` và cách phân biệt cha/con (dòng 24–27)

```c
if((pid = fork()) < 0){
  fprintf(2, "pingpong: fork failed\n");
  exit(1);
}
```

`fork()` tạo bản sao gần như hoàn chỉnh của tiến trình: cùng code, cùng dữ liệu, cùng bảng fd. Điểm khác biệt duy nhất quan trọng ở đây là **giá trị trả về**:

| `fork()` trả về | Đang ở tiến trình nào |
|---|---|
| `0` | **Con** |
| `> 0` | **Cha** (giá trị là PID của con) |
| `< 0` | Lỗi, không tạo được con |

Đây là mẹo thiết kế của UNIX: cùng một dòng code chạy ở hai tiến trình, nhưng nhận hai giá trị khác nhau, nên `if(pid == 0)` tách được hai nhánh.

Cặp ngoặc trong `(pid = fork()) < 0` là bắt buộc, cùng lý do như ở `sleep.c`: không có ngoặc thì C hiểu thành `pid = (fork() < 0)`, gán 0 hoặc 1 vào `pid`, và mọi logic sau đó sai.

Sau `fork()`, **mỗi đầu pipe có 2 tham chiếu** (một của cha, một của con) — tổng cộng 8 fd đang mở trỏ vào 2 pipe. Đây là lý do mục 5 phải đóng bớt.

---

## 5. Nhánh con (dòng 29–48)

```c
if(pid == 0){
  close(p2c[1]);
  close(c2p[0]);
```

Con **chỉ đọc** `p2c` và **chỉ ghi** `c2p`, nên đóng ngay hai đầu còn lại:

- `p2c[1]` — con không ghi vào chiều ping
- `c2p[0]` — con không đọc chiều pong (nếu không đóng, con có thể đọc lại byte pong của chính nó)

```c
  if(read(p2c[0], &buf, 1) != 1){
    fprintf(2, "pingpong: child read failed\n");
    exit(1);
  }
```

`read(fd, địa_chỉ, số_byte)` trả về **số byte thực sự đọc được**:

| Giá trị trả | Nghĩa |
|---|---|
| `1` | Đọc được 1 byte — thành công |
| `0` | **EOF** — pipe rỗng *và* mọi đầu ghi đã đóng |
| `-1` | Lỗi (fd không hợp lệ, tiến trình bị kill…) |

Kiểm `!= 1` nên bắt được cả `0` lẫn `-1` bằng một điều kiện.

**Điểm mấu chốt: `read` ở đây là lời gọi chặn (blocking).** Lúc con chạy tới dòng này, cha có thể còn chưa kịp ghi gì. Khi đó pipe rỗng nhưng đầu ghi vẫn mở, nên kernel **đưa con vào trạng thái ngủ** chứ không trả về 0. Code tương ứng trong `kernel/pipe.c`:

```c
while(pi->nread == pi->nwrite && pi->writeopen){  // rỗng VÀ còn người ghi
  if(killed(pr)){ release(&pi->lock); return -1; }
  sleep(&pi->nread, &pi->lock);                   // ngủ, nhả lock
}
```

Chú ý điều kiện `&& pi->writeopen`: pipe rỗng mà **vẫn còn** đầu ghi mở thì ngủ chờ; pipe rỗng mà **hết** đầu ghi thì thoát vòng lặp và trả về 0 = EOF. Đây chính là cơ chế mà bài `primes` sẽ dùng để biết khi nào pipeline kết thúc.

Lại là mẫu "ngủ trong vòng `while` kiểm điều kiện" giống `sys_sleep` ở bài trước.

```c
  printf("%d: received ping\n", getpid());
```

`getpid()` là system call trả về PID của **tiến trình đang gọi** — ở đây là con. Đề yêu cầu in đúng định dạng `<pid>: received ping`.

```c
  if(write(c2p[1], &buf, 1) != 1){ ... }
```

Gửi trả **chính byte vừa nhận** (`buf`). Có thể gửi byte bất kỳ vì nội dung không quan trọng, nhưng gửi lại byte cũ thể hiện đúng ý "ping-pong": cùng một quả bóng được đánh qua đánh lại.

`write` trả về số byte đã ghi. Nó cũng có thể chặn nếu pipe đầy (512 byte), nhưng ở đây chỉ ghi 1 byte vào pipe rỗng nên không bao giờ chặn.

```c
  close(p2c[0]);
  close(c2p[1]);
  exit(0);
}
```

Đóng nốt hai fd còn lại rồi thoát. Thực ra `exit()` tự đóng mọi fd, nên hai dòng `close` này là **thừa về mặt chức năng**. Giữ lại vì hai lý do: thể hiện rõ ý "đã dùng xong", và tập thói quen cần thiết cho bài `primes` — ở đó quên một `close` là treo cả pipeline.

`exit(0)` bắt buộc phải có. Nếu thiếu, con sẽ chạy tiếp xuống phần code của cha ở dưới và in thêm dòng "received pong" sai.

---

## 6. Nhánh cha (dòng 50–68)

```c
close(p2c[0]);
close(c2p[1]);
```

Đối xứng với con: cha chỉ ghi `p2c`, chỉ đọc `c2p`.

Đến đây mỗi chiều chỉ còn đúng một đầu ghi và một đầu đọc:

```
p2c:  cha[1] ghi  --->  con[0] đọc
c2p:  con[1] ghi  --->  cha[0] đọc
```

```c
if(write(p2c[1], "x", 1) != 1){ ... }
```

Gửi 1 byte khởi động. `"x"` là chuỗi hằng, trong C tên chuỗi chính là địa chỉ ký tự đầu, nên truyền thẳng vào được. Nội dung byte không quan trọng — đề chỉ yêu cầu "trao đổi một byte".

```c
if(read(c2p[0], &buf, 1) != 1){ ... }
printf("%d: received pong\n", getpid());
```

Cha chặn ở đây cho đến khi con ghi xong. **Chính sự chặn này đảm bảo thứ tự in ra**: cha không thể in "pong" trước khi con in "ping", vì cha còn đang ngủ chờ dữ liệu mà con chỉ gửi sau khi đã in. Thứ tự output được bảo đảm bởi **quan hệ phụ thuộc dữ liệu**, không phải nhờ `sleep` hay may mắn.

```c
close(p2c[1]);
close(c2p[0]);
wait(0);
exit(0);
```

`wait(0)` chặn cho đến khi một tiến trình con kết thúc. Tham số là con trỏ để nhận mã thoát của con; truyền `0` (tức `NULL`) nghĩa là **không quan tâm** mã thoát.

Vì sao cần `wait`? Khi con gọi `exit()`, nó chưa biến mất hẳn — nó chuyển sang trạng thái **ZOMBIE**, giữ lại một ô trong bảng `proc[NPROC]` để lưu mã thoát cho cha đọc. Chỉ khi cha gọi `wait()` thì ô đó mới được giải phóng. Cha thoát mà không `wait` thì con được `init` nhận nuôi và `init` sẽ dọn hộ — nên bài này không `wait` vẫn chạy đúng. Nhưng để lại zombie là thói quen xấu, và bài `primes` **bắt buộc** phải `wait` vì đề yêu cầu "tiến trình primes chính chỉ thoát sau khi tất cả tiến trình khác đã thoát".

---

## 7. Luồng chạy đầy đủ

Dòng thời gian, giả sử cha PID 4, con PID 5:

```
CHA (pid 4)                          CON (pid 5)
-----------------------------------  -----------------------------------
pipe(p2c) -> fd 3(đọc), 4(ghi)
pipe(c2p) -> fd 5(đọc), 6(ghi)
fork() ---------------------------->  (sinh ra, thừa hưởng fd 3,4,5,6)
pid = 5  (khác 0 -> nhánh cha)        pid = 0  (-> nhánh con)

close(3)  bỏ đầu đọc p2c              close(4)  bỏ đầu ghi p2c
close(6)  bỏ đầu ghi c2p              close(5)  bỏ đầu đọc c2p
   còn: 4 (ghi ping), 5 (đọc pong)       còn: 3 (đọc ping), 6 (ghi pong)

                                      read(3,...)
                                         pipe rỗng, còn đầu ghi mở
                                         -> NGỦ (kernel: sleep(&nread))
write(4, "x", 1)
   đặt byte vào bộ đệm pipe
   kernel: wakeup(&nread) ---------->  THỨC DẬY, đọc được 1 byte
read(5,...)                           printf("5: received ping")
   pipe c2p rỗng, đầu ghi còn mở
   -> NGỦ                             write(6, &buf, 1)
                                         kernel: wakeup(&nread)
   <------------------------------------ THỨC DẬY
đọc được 1 byte                       close(3); close(6)
printf("4: received pong")            exit(0)  -> trạng thái ZOMBIE
close(4); close(5)
wait(0)  thu hoạch con ------------->  ô proc được giải phóng, con biến mất
exit(0)
```

Kết quả in ra màn hình:

```
5: received ping
4: received pong
```

---

## 8. Kiểm thử đã thực hiện

**Autograder** — `./grade-lab-util pingpong`:

```
== Test pingpong == pingpong: OK (1.2s)
```

Đạt 20/20 điểm. Test này khớp ba mẫu: `^\d+: received ping$`, `^\d+: received pong$`, và `^OK$` của lệnh `echo OK` chạy ngay sau — mẫu thứ ba xác nhận chương trình **thoát hẳn** chứ không treo.

**Thủ công trong QEMU**, chạy hai lần liên tiếp:

```
$ pingpong
5: received ping
4: received pong
$ pingpong
7: received ping
6: received pong
$ echo DONE
DONE
```

PID tăng dần qua mỗi lần chạy (4,5 rồi 6,7) đúng như mong đợi; con luôn lớn hơn cha đúng 1. Thứ tự ping trước pong ổn định ở cả hai lần.

---

## 9. Khó khăn và ghi chú cho báo cáo

**1. Hiểu sai ban đầu: tưởng một pipe là đủ.** Pipe một chiều nên phản xạ đầu tiên là "cần hai chiều thì dùng hai pipe" — nhưng lý do sâu hơn mới quan trọng: ngay cả khi chấp nhận dùng một pipe cho cả hai chiều, `fork()` nhân đôi bảng fd khiến cả hai tiến trình đều cầm cả hai đầu, nên một tiến trình có thể đọc lại byte của chính mình. Đây là race condition, chạy thử vài lần vẫn có thể ra đúng rồi mới sai sau. Nhóm chọn hai pipe + đóng đầu không dùng để loại bỏ race **bằng cấu trúc** thay vì dựa vào thời điểm.

**2. Thứ tự in ping trước pong đến từ đâu.** Lúc đầu lo phải thêm `sleep` hay `wait` để ép thứ tự. Thực ra không cần: `read` là lời gọi chặn, cha ngủ trong kernel cho tới khi con ghi, mà con chỉ ghi sau khi đã in "ping". Thứ tự được bảo đảm bởi quan hệ phụ thuộc dữ liệu. Thêm `sleep` để "cho chắc" là cách sửa sai — nó che lỗi chứ không sửa lỗi.

**3. `close` thừa nhưng vẫn giữ.** Hai dòng `close` cuối mỗi nhánh không cần thiết vì `exit()` tự đóng mọi fd. Giữ lại có chủ ý để tập thói quen cho bài `primes`, nơi đề cảnh báo rõ: quên `close` thì "chương trình sẽ chạy hết tài nguyên trước khi tiến trình đầu tiên đạt đến 280".

**4. Liên hệ sang bài `primes`.** Điều kiện `while(pi->nread == pi->nwrite && pi->writeopen)` trong `kernel/pipe.c` là nền tảng của bài kế tiếp: `read` chỉ trả về `0` (EOF) khi pipe rỗng **và** mọi đầu ghi đã đóng. Mỗi fd chưa đóng là một đầu ghi còn mở, đủ để giữ cho `read` ngủ mãi mãi thay vì báo EOF. Đây là lý do `primes` sống chết vì kỷ luật đóng fd.
