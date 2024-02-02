output "public_ip_address" {
  value = "http://${azurerm_public_ip.my_public_ip.ip_address}"
}

output "public_key" {
  value = jsondecode(azapi_resource_action.ssh_public_key_gen.output).publicKey
}

output "private_key" {
    value = jsondecode(azapi_resource_action.ssh_public_key_gen.output).privateKey
}