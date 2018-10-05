use std::io;

fn main() {
    println!("Type a number, fucker!");
    loop {
        let mut var = String::new();
        io::stdin().read_line(&mut var)
         .expect("NO TYPING?????");
        let var : u32 = match var.trim().parse() {
            Ok(num) => num,
            Err(_) => continue,
        };

        println!("Oh boi, you type {}?", var);

        println!("NOW TYPE AGAIN!!!!")
    }
}
