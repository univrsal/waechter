import { defineConfig } from 'vitepress';

export default defineConfig({
  lang: 'en-US',
  title: 'Wächter',
  description: 'Traffic shaping for GNU/Linux',

  lastUpdated: true,
  cleanUrls: true,

  head: [['link', { rel: 'icon', type: 'image/png', href: '/icon.png' }]],

  themeConfig: {
    nav: [{ text: 'Docs', link: '/docs/' }],

    sidebar: {
      '/guide/': [
        {
          text: 'Guide',
          items: [
            { text: 'Introduction', link: '/guide/' },
            { text: 'Getting Started', link: '/guide/getting-started' },
          ],
        },
      ],
    },

    socialLinks: [
      // Add your repo link when you want it visible in the header:
      { icon: 'github', link: 'https://github.com/univrsal/waechter' },
      { icon: 'discord', link: 'https://discord.gg/kZsHuncu3q' },
    ],
  },
});
