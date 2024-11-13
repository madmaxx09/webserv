/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gt-serst <gt-serst@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/08 10:35:22 by gt-serst          #+#    #+#             */
/*   Updated: 2024/07/03 13:36:06 by gt-serst         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "parser/confParser.hpp"
#include "exec/ServerManager.hpp"
#include "exec/Server.hpp"
#include "exec/ServerManager.hpp"
#include "request/Request.hpp"
#include "response/Response.hpp"
#include "response/Router.hpp"

int main(int argc, char **argv)
{
	return (webserv(argc, argv));
}
