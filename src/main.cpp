// Định nghĩa chân hoặc cấu hình mặc định
void setup() {
  // Khởi động Serial để in kết quả
  Serial.begin(115200);
  delay(1000); // Đợi một chút để Serial kết nối
  Serial.println("Kiểm tra cảm biến Hall - Bắt đầu!");
}

void loop() {
  // Đọc giá trị từ cảm biến Hall (trả về giá trị tương tự)
  int hallValue = hallRead();
  
  // In giá trị ra Serial Monitor
  Serial.print("Giá trị cảm biến Hall: ");
  Serial.println(hallValue);
  
  // Đợi 500ms trước khi đọc lại
  delay(500);
}