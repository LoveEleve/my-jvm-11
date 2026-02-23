// 测试用 Java 源文件，用 javac 编译后作为 ClassFileParser 的输入
public class HelloWorld {
    private int count;
    public static final String MESSAGE = "Hello, Mini JVM!";

    public HelloWorld() {
        this.count = 0;
    }

    public int add(int a, int b) {
        return a + b;
    }

    public static void main(String[] args) {
        HelloWorld hw = new HelloWorld();
        int result = hw.add(3, 4);
        System.out.println(result);
    }
}
