"use client";
import Link from "next/link";
import { usePathname } from "next/navigation"; // Importa usePathname
import { FaTachometerAlt } from "react-icons/fa";
import { SiPowershell } from "react-icons/si";

const menuItems = [
  {
    name: "Dashboard",
    path: "/",
    icon: <FaTachometerAlt size={20} className="m-auto" />,
  },
  {
    name: "Logs",
    path: "/logs",
    icon: <SiPowershell size={20} className="m-auto" />,
  },
];

export function Menu() {
  const pathname = usePathname(); // Obtém o caminho atual

  return (
    <nav className="mt-6">
      <div>
        {menuItems.map((item) => (
          <Link
            key={item.path}
            href={item.path}
            className={`flex items-center justify-start w-full p-4 my-2 font-thin uppercase transition-colors duration-200 ${
              pathname === item.path
                ? "text-yellow-400 border-r-4 border-yellow-400 bg-gradient-to-r from-white to-yellow-100 dark:from-gray-700 dark:to-gray-800"
                : "text-gray-500 dark:text-gray-200 hover:text-yellow-400"
            }`}
          >
            <span className="text-left">
              {item.icon} {/* Renderiza o ícone a partir do array */}
            </span>
            <span className="mx-4 text-sm font-normal">{item.name}</span>
          </Link>
        ))}
      </div>
    </nav>
  );
}
