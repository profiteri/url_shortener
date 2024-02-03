# Create Virtual Network
resource "azurerm_virtual_network" "my_virtual_network" {
  name                = var.virtual_network_name
  address_space       = ["10.0.0.0/16"]
  location            = var.resource_group_location
  resource_group_name = var.resource_group_name
}

# Create a subnet in the Virtual Network
resource "azurerm_subnet" "my_subnet" {
  name                 = var.subnet_name
  resource_group_name  = var.resource_group_name
  virtual_network_name = azurerm_virtual_network.my_virtual_network.name
  address_prefixes     = ["10.0.1.0/24"]
}

# Create Network Security Group and rules
resource "azurerm_network_security_group" "my_nsg" {
  name                = var.network_security_group_name
  location            = var.resource_group_location
  resource_group_name = var.resource_group_name

  security_rule {
    name                       = "web"
    priority                   = 1008
    direction                  = "Inbound"
    access                     = "Allow"
    protocol                   = "Tcp"
    source_port_range          = "*"
    destination_port_range     = "80"
    source_address_prefix      = "*"
    destination_address_prefix = "10.0.1.0/24"
  }

  security_rule {
    name                       = "ssh"
    priority                   = 1000
    direction                  = "Inbound"
    access                     = "Allow"
    protocol                   = "*"
    source_port_range          = "*"
    destination_port_range     = "22"
    source_address_prefix      = "*"
    destination_address_prefix = "10.0.1.0/24"
  }

}

# Associate the Network Security Group to the subnet
resource "azurerm_subnet_network_security_group_association" "my_nsg_association" {
  subnet_id                 = azurerm_subnet.my_subnet.id
  network_security_group_id = azurerm_network_security_group.my_nsg.id
}

# Create Public IP
resource "azurerm_public_ip" "my_public_ip" {
  name                = var.public_ip_name
  location            = var.resource_group_location
  resource_group_name = var.resource_group_name
  allocation_method   = "Static"
  sku                 = "Standard"
}

# Create Network Interface
resource "azurerm_network_interface" "my_nic" {
  count               = 2
  name                = "${var.network_interface_name}${count.index}"
  location            = var.resource_group_location
  resource_group_name = var.resource_group_name

  ip_configuration {
    name                          = "ipconfig${count.index}"
    subnet_id                     = azurerm_subnet.my_subnet.id
    private_ip_address_allocation = "Dynamic"
    primary                       = true
  }
}

# Associate Network Interface to the Backend Pool of the Load Balancer
resource "azurerm_network_interface_backend_address_pool_association" "my_nic_lb_pool" {
  count                   = 2
  network_interface_id    = azurerm_network_interface.my_nic[count.index].id
  ip_configuration_name   = "ipconfig${count.index}"
  backend_address_pool_id = azurerm_lb_backend_address_pool.my_lb_pool.id
}

# Create Virtual Machine
resource "azurerm_linux_virtual_machine" "my_vm" {
  count                 = 2
  name                  = "${var.virtual_machine_name}${count.index}"
  location              = var.resource_group_location
  resource_group_name   = var.resource_group_name
  network_interface_ids = [azurerm_network_interface.my_nic[count.index].id]
  size                  = var.virtual_machine_size

  os_disk {
    name                 = "${var.disk_name}${count.index}"
    caching              = "ReadWrite"
    storage_account_type = var.redundancy_type
  }

  source_image_reference {
    publisher = "Canonical"
    offer     = "0001-com-ubuntu-server-jammy"
    sku       = "22_04-lts-gen2"
    version   = "latest"
  }

  admin_username                  = var.username
  admin_password                  = var.password
  disable_password_authentication = false

}

# Enable virtual machine extension and install Nginx
resource "azurerm_virtual_machine_extension" "my_vm_extension" {
  count                = 2
  name                 = "Nginx"
  virtual_machine_id   = azurerm_linux_virtual_machine.my_vm[count.index].id
  publisher            = "Microsoft.Azure.Extensions"
  type                 = "CustomScript"
  type_handler_version = "2.0"

  settings = <<SETTINGS
{
 "commandToExecute": "sudo apt-get update && sudo apt-get install nginx -y && echo \"Hello World from $(hostname)\" > /var/www/html/index.html && sudo systemctl restart nginx"
}
SETTINGS

}

# Create Public Load Balancer
resource "azurerm_lb" "my_lb" {
  name                = var.load_balancer_name
  location            = var.resource_group_location
  resource_group_name = var.resource_group_name
  sku                 = "Standard"

  frontend_ip_configuration {
    name                 = var.public_ip_name
    public_ip_address_id = azurerm_public_ip.my_public_ip.id
  }
}

resource "azurerm_lb_backend_address_pool" "my_lb_pool" {
  loadbalancer_id = azurerm_lb.my_lb.id
  name            = "test-pool"
}

resource "azurerm_lb_probe" "my_lb_probe" {
  resource_group_name = var.resource_group_name
  loadbalancer_id     = azurerm_lb.my_lb.id
  name                = "test-probe"
  port                = 80
}

resource "azurerm_lb_rule" "my_lb_rule" {
  resource_group_name            = var.resource_group_name
  loadbalancer_id                = azurerm_lb.my_lb.id
  name                           = "test-rule"
  protocol                       = "Tcp"
  frontend_port                  = 80
  backend_port                   = 80
  disable_outbound_snat          = true
  frontend_ip_configuration_name = var.public_ip_name
  probe_id                       = azurerm_lb_probe.my_lb_probe.id
  backend_address_pool_ids       = [azurerm_lb_backend_address_pool.my_lb_pool.id]
}

resource "azurerm_lb_nat_rule" "my_nat_rule_vm1" {
  resource_group_name            = var.resource_group_name
  loadbalancer_id                = azurerm_lb.my_lb.id
  name                           = "test-ssh-vm1"
  protocol                       = "Tcp"
  frontend_port                  = 221
  backend_port                   = 22
  frontend_ip_configuration_name = var.public_ip_name
}

resource "azurerm_lb_nat_rule" "my_nat_rule_vm2" {
  resource_group_name            = var.resource_group_name
  loadbalancer_id                = azurerm_lb.my_lb.id
  name                           = "test-ssh-vm2"
  protocol                       = "Tcp"
  frontend_port                  = 222
  backend_port                   = 22
  frontend_ip_configuration_name = var.public_ip_name
}

# Associate LB NAT Rule and VM Network Interface
resource "azurerm_network_interface_nat_rule_association" "my_web_nic_nat_rule_associate_vm1" {
  network_interface_id  = azurerm_network_interface.my_nic[0].id
  ip_configuration_name = azurerm_network_interface.my_nic[0].ip_configuration[0].name
  nat_rule_id           = azurerm_lb_nat_rule.my_nat_rule_vm1.id
}

# Associate LB NAT Rule and VM Network Interface
resource "azurerm_network_interface_nat_rule_association" "my_web_nic_nat_rule_associate_vm2" {
  network_interface_id  = azurerm_network_interface.my_nic[1].id
  ip_configuration_name = azurerm_network_interface.my_nic[1].ip_configuration[0].name
  nat_rule_id           = azurerm_lb_nat_rule.my_nat_rule_vm2.id
}

resource "azurerm_lb_outbound_rule" "my_lboutbound_rule" {
  resource_group_name     = var.resource_group_name
  name                    = "test-outbound"
  loadbalancer_id         = azurerm_lb.my_lb.id
  protocol                = "Tcp"
  backend_address_pool_id = azurerm_lb_backend_address_pool.my_lb_pool.id

  frontend_ip_configuration {
    name = var.public_ip_name
  }
}