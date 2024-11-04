use binary_layout::prelude::*;
use std::io::{Read, Write};

binary_layout!(message_header, BigEndian, {
    message_type: u8,
    _reserved: [u8; 3],
    content_length: i32,
});

binary_layout!(message_body, BigEndian, {
    content: [u8],
});

binary_layout!(message, BigEndian, {
    header: message_header::NestedView,
    body: message_body::NestedView,
});

#[derive(Debug)]
enum MessageType {
    Ok,
    Write,
    Clear,
    Error,
    Ping,
}

impl TryFrom<u8> for MessageType {
    type Error = String;

    fn try_from(v: u8) -> Result<Self, String> {
        let value = match v {
            0x1 => MessageType::Ok,
            0x2 => MessageType::Clear,
            0x3 => MessageType::Write,
            0x4 => MessageType::Error,
            0x5 => MessageType::Ping,
            other => { return Err(format!("Message type not recognized: {other}")); },
        };
        Ok(value)
    }
}

impl Into<u8> for MessageType {
    fn into(self) -> u8 {
        match self {
            MessageType::Ok => 0x1,
            MessageType::Clear => 0x2,
            MessageType::Write => 0x3,
            MessageType::Error => 0x4,
            MessageType::Ping => 0x5,
        }
    }
}

#[derive(Debug)]
pub enum Message {
    Ok,
    Write { content: Vec<u8> },
    Clear,
    Error { message: Option<Vec<u8>> },
    Ping,
}

impl Into<MessageType> for Message {
    fn into(self) -> MessageType {
        match self {
            Message::Ok => MessageType::Ok,
            Message::Clear => MessageType::Clear,
            Message::Write { content: _ } => MessageType::Write,
            Message::Error { message: _ } => MessageType::Error,
            Message::Ping => MessageType::Ping,
        }
    }
}

pub fn read_message(stream: &mut impl Read) -> Result<Option<Message>, String> {
    let mut buffer = vec![0; 32];
    let size = stream.read(&mut buffer).expect("Couldn't read");
    if size == 0 { return Ok(None); };
    let binary_message = message_header::View::new(buffer);

    let content_length: usize = binary_message.content_length().read().try_into().unwrap();
    let mut body = vec![0; 0];
    if content_length != 0 {
        body.resize(content_length, 0);
        let size = stream.read(&mut body).expect("Couldn't read body");
        if size != content_length { return Err(format!("Message error: body is {} bytes, header states {}", size, content_length)); }
    }
    // Convert the body to network byte order
    let body = message_body::content::data(&body).to_vec();

    let message_type: MessageType = binary_message.message_type().read().try_into()?;

    match message_type {
        MessageType::Write | MessageType::Error => {

        },
        ref other => {
            if content_length != 0 { return Err(format!("Content Length of {:?} must be 0", other)); }
        },
    };

    let result = match message_type {
        MessageType::Ok => Message::Ok,
        MessageType::Write => Message::Write { content: body },
        MessageType::Clear => Message::Clear,
        MessageType::Error => Message::Error { message :
            if content_length == 0 { None }
            else { Some(body) }
        },
        MessageType::Ping => Message::Ping,
    };
    Ok(Some(result))
}

pub fn write_message(stream: &mut impl Write, message: Message) {
    let (content_length, content): (usize, Option<&Vec<u8>>) = match message {
        Message::Ok | Message::Clear | Message::Ping => (0, None),
        Message::Write { content: ref content } => (content.len(), Some(&content)),
        Message::Error { ref message } => match message {
            Some(msg) => (msg.len(), Some(msg)),
            None => (0, None),
        }
    };
    let message_type: MessageType = message.into();
    let message_code: u8 = message_type.into();

    let mut buffer = vec![0; content_length + 8];
    let mut message = message::View::new(&mut buffer[..]);

    message.header_mut().message_type_mut().write(message_code);
    message.header_mut().content_length_mut().write(content_length.try_into().unwrap());
    
    if let Some(content) = content {
        message.body_mut().content_mut().copy_from_slice(&content);
    }

    let size = stream.write(&buffer).expect("Couldn't write");
    println!("Wrote message of size {}", size);
}

/*
pub fn write_message(message_type: u8, content: Option<&[u8]>) -> Vec<u8> {
    if let Some(c) = content {
        message.body_mut().content_mut().copy_from_slice(c);
    }
    storage
}
*/
