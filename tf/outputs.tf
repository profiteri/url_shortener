output "public_ip_address" {
  value = "http://${azurerm_public_ip.my_public_ip.ip_address}"
}