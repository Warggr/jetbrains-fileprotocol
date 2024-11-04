use std::env;
use std::fs::File;
use std::os::unix::net::{UnixListener, UnixStream};
use std::io::prelude::*;
use std::process;

mod serialization;

use crate::serialization::{Message, read_message, write_message};

fn parse_args() -> Result<(String, String), String> {
    let args: Vec<String> = env::args().collect();

    if args.len() != 3 { return Err(format!("Wrong number of arguments: expected 2, got {}", args.len())); }
    let file = args[1].clone();
    let socket = args[2].clone();

    Ok((file, socket))
}

fn print_help(err: Option<String>) {
    println!("usage: fileprotocol [socket path] [file path]");
    match err {
        Some(msg) => println!("{}", msg),
        None => (),
    }
}

fn handle<'a>(request: Message) -> Message {
    request
}

fn handle_client(mut stream: UnixStream) {
    loop {
        let message = read_message(&mut stream);
        let response = match message {
            Err(msg) => Message::Error { message: Some(msg.into_bytes().to_vec()) },
            Ok(None) => { return; } // no message could be read because stream ended
            Ok(Some(msg)) => handle(msg),
        };
        write_message(&mut stream, response);
    }
}

fn main() {
    let (file, socket) = parse_args().unwrap_or_else(|err| {
        print_help(Some(err));
        process::exit(1);
    });

    let mut file = File::create(file.clone()).unwrap_or_else(|err| {
        println!("Couldn't open file `{file}`: {err}");
        process::exit(2);
    });
    let listener = UnixListener::bind(socket.clone()).unwrap_or_else(|err| {
        println!("Couldn't open socket `{socket}`: {err}");
        process::exit(2);
    });

    match listener.accept() {
        Ok((socket, addr)) => handle_client(socket),
        Err(e) => println!("accept function failed: {e:?}"),
    }
    /*
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                handle_client(stream);
            }
            Err(_err) => {
                /* connection failed */
                break;
            }
        }
    }
    */
}
