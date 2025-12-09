'use client';

import { ReactNode } from 'react';
import { Sidebar } from '@/components/sidebar';

interface LayoutProps {
  children: ReactNode;
  wsConnected?: boolean;
}

/**
 * Unified layout component for all pages
 * Provides consistent sidebar navigation across the app
 */
export function Layout({ children, wsConnected }: LayoutProps) {
  return (
    <div className="flex h-screen bg-gray-50 dark:bg-slate-950">
      {/* Sidebar Navigation */}
      <Sidebar wsConnected={wsConnected} />

      {/* Main Content Area */}
      <div className="flex-1 overflow-auto">
        {children}
      </div>
    </div>
  );
}

interface PageHeaderProps {
  title: string;
  description?: string;
  action?: ReactNode;
}

/**
 * Reusable page header component
 */
export function PageHeader({ title, description, action }: PageHeaderProps) {
  return (
    <div className="border-b bg-white dark:bg-slate-900 px-8 py-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-gray-900 dark:text-white">
            {title}
          </h1>
          {description && (
            <p className="mt-1 text-sm text-gray-600 dark:text-gray-400">
              {description}
            </p>
          )}
        </div>
        {action && <div>{action}</div>}
      </div>
    </div>
  );
}
