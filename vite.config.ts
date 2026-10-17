import { defineConfig } from 'vite-plus'

// Shared fmt/lint settings for every workspace. app/characters and docs are
// upstream content, left as they came.
const ignorePatterns = ['**/dist/**', '**/node_modules/**', '**/.pio/**', 'release/**', 'web/public/release/**', 'app/characters/**', 'docs/**']

export default defineConfig({
  fmt: {
    singleQuote: true,
    semi: false,
    trailingComma: 'all',
    tabWidth: 2,
    printWidth: 150,
    ignorePatterns,
  },
  lint: {
    ignorePatterns,
    rules: {
      'no-unused-vars': ['warn'],
      'typescript/unbound-method': 'off',
      'typescript/no-unused-vars': ['warn', { varsIgnorePattern: '^_', argsIgnorePattern: '^_', caughtErrorsIgnorePattern: '^_' }],
    },
    options: {
      typeAware: true,
      typeCheck: true,
    },
  },
  staged: {
    '*': 'vp check --fix',
  },
})
