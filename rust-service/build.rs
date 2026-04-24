fn main() -> Result<(), Box<dyn std::error::Error>> {
    let proto_dir = if std::path::Path::new("proto/tick.proto").exists() {
        "proto"
    } else {
        "../proto"
    };

    tonic_build::configure()
        .build_server(true)
        .build_client(false)
        .compile(
            &[format!("{}/tick.proto", proto_dir)],
            &[proto_dir],
        )?;
    Ok(())
}
