# user/primes.c — thiết kế và luồng chạy

> Comment ngắn nằm trong `user/primes.c`. Tài liệu này ghi phần **vì sao**, luồng chạy, và kiểm thử.

## Chức năng

`primes` in ra mọi số nguyên tố từ 2 đến 280, mỗi số một dòng dạng `prime <n>`. Không nhận đối số.

Đề yêu cầu cài theo mô hình **sàng dạng ống (concurrent prime sieve)** của Doug McIlroy, dùng `pipe` và `fork`. Đây không phải yêu cầu về kết quả mà về **cách làm**: một vòng lặp kiểm tra chia hết cũng cho ra cùng danh sách, nhưng không đạt yêu cầu đề.

## Ý tưởng

Thay vì một chương trình sàng, dựng một **dây chuyền tiến trình**. Mỗi tiến trình phụ trách đúng một số nguyên tố, lọc bỏ bội số của nó, và đẩy phần còn lại sang tiến trình sau.

```
[sinh 2..280] -> [lọc bội 2] -> [lọc bội 3] -> [lọc bội 5] -> ...
                  in "prime 2"   in "prime 3"   in "prime 5"
```

Ba quan sát làm mô hình này chạy được:

**1. Số đầu tiên một tầng đọc được luôn là số nguyên tố.** Mọi số nhỏ hơn nó đã bị các tầng trước lọc sạch, nên không cần kiểm tra gì. Đây là điểm tinh tế nhất của thuật toán: việc kiểm tra nguyên tố biến mất hoàn toàn, nó là hệ quả của cấu trúc dây chuyền.

**2. Mỗi tầng chỉ cần nhớ một số.** Không mảng, không `malloc`. Trạng thái nằm ở chỗ *tiến trình nào đang chạy*, không nằm trong dữ liệu.

**3. Dây chuyền tự dài ra.** Tầng nào gặp số sống sót đầu tiên thì sinh tầng sau.

Với giới hạn 280, dây chuyền dài 59 tầng, đúng bằng số lượng số nguyên tố không vượt quá 280.

---

## Quyết định thiết kế 1 — đệ quy thay vì vòng lặp

Có hai cách dựng dây chuyền: hàm đệ quy tự gọi lại chính nó trong tiến trình con, hoặc vòng lặp dựng từng tầng.

Nhóm chọn đệ quy vì chính đề đã ngầm chỉ hướng đó. Gợi ý trong đề *"nếu gcc báo lỗi đệ quy vô hạn, khai báo `void primes(int) __attribute__((noreturn))`"* chỉ có ý nghĩa với bản đệ quy.

Điểm cần hiểu rõ: đây **không phải đệ quy thông thường**. Mỗi lần gọi lại xảy ra trong một **tiến trình khác**, không phải một khung stack mới của cùng tiến trình. Nên không có nguy cơ tràn stack, nhưng có nguy cơ cạn bảng tiến trình.

Thuộc tính `noreturn` báo cho trình biên dịch biết hàm luôn kết thúc bằng `exit`, nhờ vậy nó không cảnh báo đệ quy vô hạn.

## Quyết định thiết kế 2 — fork lười

Đề ghi rõ *"chỉ nên tạo tiến trình khi cần thiết"*.

Nếu mỗi tầng fork ngay khi khởi động, dây chuyền sinh tiến trình không có điểm dừng: tầng cuối không nhận được số nào nhưng vẫn đẻ tầng mới. xv6 giới hạn `NPROC` bằng 64 tiến trình, nên chương trình sẽ chết trước khi đạt 280.

Nhóm dùng biến `pid` khởi tạo bằng -1 làm cờ. Tầng sau chỉ được dựng ở **lần đầu tiên** có một số vượt qua được bộ lọc. Số nguyên tố cuối cùng trong dải không có số nào sống sót nên không sinh tầng thừa.

## Quyết định thiết kế 3 — kỷ luật đóng file descriptor

Đây là phần làm hỏng bài nhiều nhất. Đề cảnh báo *"cẩn thận khi đóng file descriptor, nếu không chương trình sẽ chạy hết tài nguyên trước khi đạt 280"*.

Cơ chế nền tảng đã gặp ở bài `pingpong`: hàm `piperead` trong `kernel/pipe.c` chỉ trả về 0 khi pipe rỗng **và** mọi đầu ghi đã đóng. Một file descriptor chưa đóng tính là một đầu ghi còn mở, đủ để `read` ngủ vĩnh viễn thay vì báo hết dữ liệu.

Với 59 tầng, hậu quả nhân lên theo hai hướng: cạn file descriptor, và tệ hơn là **treo toàn bộ dây chuyền**.

Ba chỗ `close` bắt buộc:

| Chỗ | Ai đóng | Hậu quả nếu quên |
|---|---|---|
| `close(left)` trong nhánh con ngay sau `fork` | tầng mới sinh | Tầng mới giữ thêm một đầu đọc của pipe phía trước, tích lũy qua 59 tầng |
| `close(right[0])` sau `fork` | tầng cha | Cha giữ đầu đọc pipe của chính mình, tự chặn EOF của con |
| `close(right[1])` sau vòng lặp | tầng cha | **Nghiêm trọng nhất.** Tầng sau không bao giờ thấy EOF, cả dây chuyền treo |

## Quyết định thiết kế 4 — truyền số nguyên 4 byte

Đề gợi ý ghi thẳng số nguyên 32 bit vào pipe thay vì dạng ASCII. Nhóm làm theo: mỗi lần đọc và ghi đúng `sizeof(int)` byte.

Lý do: dạng ASCII cần dấu phân cách giữa các số và cần phân tích chuỗi ở đầu nhận, thêm hai nguồn lỗi mà không được lợi gì. Dạng nhị phân có kích thước cố định nên đọc ghi đơn giản và không nhập nhằng.

## Quyết định thiết kế 5 — tiến trình gốc đợi cả dây chuyền

Đề yêu cầu *"tiến trình primes chính chỉ nên thoát sau khi tất cả đầu ra đã được in và sau khi tất cả các tiến trình primes khác đã thoát"*.

Nếu gốc thoát sớm, shell in lại dấu nhắc trong khi dây chuyền còn đang in số, làm output lẫn lộn.

Mỗi tầng gọi `wait(0)` trước khi `exit`, tạo hiệu ứng dây chuyền ngược: tầng 59 thoát trước, tầng 58 đang chờ nó nên thức dậy và thoát, cứ thế lùi về tới gốc. Gốc là tiến trình thoát cuối cùng.

Kiểm chứng được bằng cách chạy `primes` rồi `echo XONG`: dòng `XONG` phải xuất hiện sau toàn bộ danh sách số nguyên tố.

---

## Luồng chạy

Quá trình với ba tầng đầu:

```
Tiến trình gốc
  tạo pipe p, fork
  con -> loc(p[0])
  cha: ghi 2,3,4,5,...,280 vào p[1], đóng p[1], wait

Tầng 1 (lọc bội 2)
  đọc số đầu tiên: 2  -> in "prime 2"
  đọc 3: không chia hết cho 2 -> lần đầu có số sót, dựng tầng 2, gửi 3
  đọc 4: chia hết cho 2 -> bỏ
  đọc 5: gửi sang tầng 2
  ...
  hết số -> đóng đầu ghi, wait tầng 2, exit

Tầng 2 (lọc bội 3)
  đọc số đầu tiên: 3  -> in "prime 3"
  đọc 5: không chia hết cho 3 -> dựng tầng 3, gửi 5
  đọc 7: gửi
  đọc 9: bỏ
  ...

Tầng 3 (lọc bội 5)
  đọc số đầu tiên: 5  -> in "prime 5"
  ...
```

Dây chuyền đạt 59 tầng. Tầng cuối phụ trách số 277, không còn số nào sống sót nên không sinh tầng tiếp theo.

Các nhánh dừng:

| Tình huống | Xử lý |
|---|---|
| Tầng đọc không được số nào | Đóng đầu vào, `exit(0)` ngay, không in gì |
| Hết số giữa chừng | Thoát vòng lặp, đóng đầu ghi, `wait` tầng sau |
| Tầng không sinh tầng sau | `pid` vẫn bằng -1, bỏ qua phần `close(right[1])` và `wait` |
| `pipe` hoặc `fork` thất bại | In lỗi ra fd 2, `exit(1)` |

---

## Kiểm thử

Autograder, `./grade-lab-util primes`:

```
== Test primes == primes: OK (1.3s)
```

Đạt 20/20 điểm. Test này khớp danh sách 57 số nguyên tố từ 2 đến 269 mà MIT liệt kê sẵn, cộng mẫu `^OK$` của lệnh `echo OK` chạy ngay sau để xác nhận chương trình thoát hẳn chứ không treo.

Thủ công trong shell xv6, chạy `primes` rồi `echo XONG`. Kết quả: 59 dòng từ `prime 2` tới `prime 277`, sau đó mới tới `XONG`.

```
$ primes
prime 2
prime 3
prime 5
prime 7
prime 11
...
prime 271
prime 277
$ echo XONG
XONG
```

Thứ tự này xác nhận tiến trình gốc đã đợi toàn bộ dây chuyền.

Số lượng kiểm chứng được: 59 là đúng số lượng số nguyên tố không vượt quá 280, và số lớn nhất là 277 vì 278, 279, 280 đều là hợp số.

Lệnh chạy lại:

```sh
python3 xv6run.py 'primes' 'echo XONG'
```

---

## Khó khăn và ghi chú cho báo cáo

**1. Hiểu sai ban đầu về bản chất đệ quy.** Hàm `loc` tự gọi lại chính nó, nhìn giống đệ quy thường, nhưng mỗi lần gọi xảy ra trong một tiến trình khác chứ không phải một khung stack mới. Hiểu nhầm chỗ này dẫn tới lo sai: lo tràn stack (không xảy ra) thay vì lo cạn bảng tiến trình (mới là rủi ro thật).

**2. Nguy cơ treo vì quên đóng file descriptor.** Nhóm đã chuẩn bị trước nhờ bài `pingpong`, nơi đã phân tích kỹ điều kiện EOF của pipe. Nhờ vậy ba chỗ `close` được đặt đúng ngay từ đầu và chương trình chạy đúng ở lần chạy thử đầu tiên. Nếu làm `primes` trước `pingpong` thì gần như chắc chắn phải debug một ca treo.

**3. Giới hạn 280 không tùy tiện.** Dây chuyền dài bằng số lượng số nguyên tố trong dải, tức 59 tiến trình, cộng tiến trình gốc và shell. xv6 cho tối đa `NPROC` bằng 64 tiến trình. Nghĩa là bài này chạy sát trần có chủ ý của người ra đề. Nếu fork không lười, hoặc nếu đổi 280 thành số lớn hơn nhiều, chương trình sẽ chết vì hết tiến trình.

**4. Chênh lệch giữa đề và autograder.** Đề yêu cầu sinh số tới 280, còn danh sách kiểm tra của MIT trong `grade-lab-util` chỉ liệt kê tới 269. Nhóm theo đề, sinh tới 280, nên in ra thêm hai số 271 và 277 so với danh sách của MIT. Điều này không làm sai test vì autograder chỉ kiểm các số nó liệt kê có xuất hiện hay không, chứ không cấm in thêm.
